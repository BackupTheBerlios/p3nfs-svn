//
// This file is part of p3nfs
//
// some history
//
// 5.19    25. Oct 2005   rkoenig   
// 5.18    05. May 2005   rkoenig   Changing the version number
// 2.9     ????????????   rkoenig   A920/925 keys added, async write disabled
// 2.8     22. Jan 2004   rkoenig   BT Infoprint removed, XXX flush
// 2.7     28. Dec 2003   rkoenig   UTF8, BT infoprint
// 2.6     04. Apr 2003   rkoenig   Higher timeout (8K blocksize)
// 2.5     04. Apr 2003   rkoenig   Tcpip port
// 2.4     04. Mar 2003   rkoenig   Async read & write + 8k Blocksize
// 2.3     01. Feb 2003   rkoenig   P800 port
// 2.2     27. Jan 2003   rkoenig   File-End checking
// 2.1     XX. Sep 2002   rkoenig   bluetooth
// 2.0     16. May 2002   rkoenig   removing global variables, adding infrared
// 1.0-c   17. Nov 2001   alfredh   stop PLP before comport is opened
// 1.0-b   16. Nov 2001   alfredh   added support for autoexecution
// 1.0-a   19. Oct 2001   alfredh   fixed some minor bugs
//

#include <e32cons.h>
#include <f32file.h>
#include <c32comm.h>
#include <w32std.h>
#if defined(_PLP)
#include <plpsess.h>
#endif
#ifdef _BLUETOOTH
#include "rfcomm.h"
#endif
#ifdef _TCP
#include "tcpip.h"
#endif
#include "nfsclient.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdarg.h>
#include "util.h"
#include "../../../server/version.h"


#define BLOCKSIZE 8192
#define CMDBUF 256
#define NCONFIG 10

#define debug0(l,a)       if(gd->debug>=l) gd->cons->Printf(_L(a))
#define debug1(l,a,b)     if(gd->debug>=l) gd->cons->Printf(_L(a), b)
#define debug2(l,a,b,c)   if(gd->debug>=l) gd->cons->Printf(_L(a), b, c)

#if defined(_UNICODE)
# define CHAR TUint16
# define _CL(a, b) (TUint16 *)char2wchar(a, b)
# define _C(a, b) TPtrC(_CL(a, b))
# define _c(a, b) wchar2char(a.Ptr(), b, a.Length())
#else // _UNICODE
# define CHAR char
# define _CL(a, b) (unsigned char *)a
# define _C(a, buf) _L(a)
# define _c(a, b) char2char((char *)a.Ptr(), b, a.Length())
#endif // _UNICODE


typedef enum {
  T_SERIAL,
  T_IRDA,
  T_BLUE,
  T_TCP,
} t_port;



/////////////////////////////////////////////
// Our global data
struct global_data { 
  TPtr8 readbuf;
  unsigned char readbufbuf[BLOCKSIZE];

  MComm *iCommPort;
  RFs *iFs;

  // Read speedup...
  char rfilename[KMaxFileName];	// Last read filename
  RFile *iRFile;	// last read-opened file
  char rbuf[16];	// Last file attributes

  // Write speedup...
  char wfilename[KMaxFileName];	// Last write filename
  RFile *iWFile;	// last write-opened file

  CConsoleBase *cons;
  int debug;
  int auto_exec;

  RTimer *iTimer;

  //////////
  // buffers
#ifdef _UNICODE
  CHAR ubuf1[CMDBUF], ubuf2[CMDBUF];
#endif

  char cbuf[CMDBUF];

  int gb_idx; ///< global buffer index

  char asyncbuf[BLOCKSIZE];
  TRequestStatus asyncst;
  int asyncop, asyncoffset;

  char doreqbuf[BLOCKSIZE]; // It shouldnt be put on the stack
  char config[CMDBUF];

  int port;
};



/////////////////////////////////
// List of IO ports
const struct {
  const char *mod;
  const char *portname;
  t_port type;
} *cp, cptable[] = {
#ifdef _CABLE
  { "ECUART",		"COMM::0",		T_SERIAL },
#endif
#ifdef _IRDA
  { "IrCOMM",		"IrCOMM::0",		T_IRDA },
#endif
#ifdef _BLUETOOTH
  { "Bluetooth",	"11",			T_BLUE },
#endif
#ifdef _TCP
  { "TCP",		"169.254.1.68:13579",	T_TCP },
#endif
};
#define NPORT (sizeof(cptable)/sizeof(*cptable))


/////////////////////////////////////////////
// Dont want to use the stdlib, so we implement the following 4 fn.
static char*
strcpy(char *a, const char *b)
{
  char *r = a;
  while((*a++ = *b++))
    ;
  return r;
}
static int
strcmp(const char *a, const char *b)
{
  int l = 0;
  while(!l && *a)
    l = *a++ - *b++;
  return l;
}
static int
strncmp(const char *a, const char *b, int len)
{
  int l = 0;
  while(len-- && !l && *a)
    l = *a++ - *b++;
  return l;
}
static int
strlen(const char *a)
{
  int l = 0;
  while(*a++)
    l++;
  return l;
}



/////////////////////////////////////////////
// Conversion function for unicode/non-unicode builds
#if defined(_UNICODE)
CHAR *
char2wchar(const char *in, CHAR *out)	// UTF8 -> UCS2
{
  CHAR *p = out;
  while(*in)
    {
      CHAR w = 0;
      if((*in & 0x80) == 0)          // 0xxx xxxx
	{
	  w = *in++;
	}
      else if((*in & 0xe0) == 0xc0)  // 110y yyyy 10xx xxxx
	{
	  w = ((in[0]&0x1f) << 6) | (in[1]&0x3f);
	  in += 2;
	}
      else if((*in & 0xf0) == 0xe0)  // 1110 zzzz 10yy yyyy 10xx xxxx
	{
	  w = ((in[0]&0xf) << 12) | ((in[1]&0x3f) << 6) | (in[2]&0x3f);
	  in += 3;
	}
      *p++ = w;
    }
  *p = 0;
  return out;
}

char *
wchar2char(const CHAR *in, char *out, int len) // UCS2 -> UTF8
{
  int i = 0, j = 0;
  while(i < len)
    {
      CHAR c = in[i++];
      if(c <= 0x7f)
	{
	  out[j++] = c;
	}
      else if(c <= 0x7ff)
	{
	  out[j++] = 0xc0 | (c >>   6); // first 5 bits
	  out[j++] = 0x80 | (c & 0x3f); // last 6 bits
	}
      else if(c <= 0x7fff)
	{
	  out[j++] = 0xe0 | (c >>  12);         // first 5 bits
	  out[j++] = 0x80 | ((c>>6) & 0x3f);    // next 6 bits
	  out[j++] = 0x80 | (c & 0x3f);         // last 6 bits
	}
    }
  out[j] = 0;
  return out;
}


#else

char *
char2char(const char *in, char *out, int len)
{
  char *o = out;

  while(len--)
    *o++ = *in++;
  *o = 0;

  return out;
}

#endif




////////////////////////////////////
// Helper functions
void
TimeoutWait(struct global_data *gd, int aMicroSec,
	TRequestStatus *st1, TRequestStatus *st2)
{
  TRequestStatus s_to;
  TTimeIntervalMicroSeconds32 t_to(aMicroSec);

  gd->iTimer->After(s_to, t_to);
  //User::WaitForRequest(s_to, st);
  while(s_to == KRequestPending &&
        (st1 == 0 || *st1 == KRequestPending) && 
        (st2 == 0 || *st2 == KRequestPending))
    User::WaitForAnyRequest();
  gd->iTimer->Cancel();
}


const char *
GetConfig(struct global_data *gd, const char *var, char *buf, const char *def)
{
  char *c = gd->config, *b;
  int l = strlen(var);
  while(*c)
    {
      if(strncmp(c, var, l) == 0 && c[l] == '=')
        {
	  c += l+1;
	  b = buf;
	  while(*c && *c != '\n')
	    *b++ = *c++;
	  *b = 0;
	  return buf;
	}
      while(*c && *c != '\n')
	c++;
      while(*c == '\n')
	c++;
    }
  return def;
}


////////////////////////////////////
// MComm to RComm translator
class MRComm : public MComm
{
public:
  ~MRComm() { iR->Close(); delete iR; }
  int  Open(CConsoleBase *cons, TDes &hostname, TDesC &destination,
  			TRequestStatus &st) { return 1; }
  void Read(TRequestStatus &aSt, TDes8 &aDes) { iR->Read(aSt, aDes); }
  void ReadOneOrMore(TRequestStatus &aSt, TDes8 &aDes)
  			{ iR->ReadOneOrMore(aSt, aDes); }
  void Write(TRequestStatus &aSt, const TDesC8 &aDes) { iR->Write(aSt, aDes); }
  void Cancel()	{ iR->Cancel(); }
  RComm *iR;
};


/////////////////////////////////////////////
// Reading from the serial line, returning a byte from a buffer
int
GetByte(struct global_data *gd)
{
  if(gd->gb_idx == gd->readbuf.Length())
    {
      gd->gb_idx = 0;
      TRequestStatus st;

      gd->iCommPort->ReadOneOrMore(st, gd->readbuf);
      TimeoutWait(gd, 3*1000*1000, &st, 0);
      if(st == KRequestPending)
        {
          gd->iCommPort->Cancel();
	  gd->readbuf.SetLength(0);
          debug0(3, " R=TO ");
          return -1000;
	}
      debug1(3, " R=%d ", gd->readbuf.Length());
    }
  return gd->readbuf[gd->gb_idx++];
}

int
DoRead(struct global_data *gd, char *buf, int len)
{
  debug1(3, " R(%d)", len);
  for(int i = 0; i < len; i++)
    {
      int r = GetByte(gd);
      if(r == -1000)
        return KErrTimedOut;
      buf[i] = (char)r;
    }
  return KErrNone;
}

/////////////////////////////////////////////
// Writing data
int
DoWrite(struct global_data *gd, const char *buf, int len)
{
  debug1(3, " W(%d)", len);

  TPtrC8 out((unsigned char *)buf, len);
  TRequestStatus st;
  gd->iCommPort->Write(st, out);
  TimeoutWait(gd, 3*1000*1000, &st, 0);
  int r = st.Int();
  if(r == KRequestPending)
    {
      debug1(3, "=%d ", r);
      gd->iCommPort->Cancel();
      return KErrTimedOut;
    }

  debug1(3, "=%d ", r);
  return r;
}

void
icp(void *t, void *f)	// Integer copy
{
  char *p1 = (char *)t;
  char *p2 = (char *)f;
  p1[0] = p2[0];
  p1[1] = p2[1];
  p1[2] = p2[2];
  p1[3] = p2[3];
}


/////////////////////////////////////////////
// Counting the filelinks & other stats
int
doStat(struct global_data *gd,
		char *fname, int namelen, char *rbuf, int isvolume)
{
  RFs *iFs = gd->iFs;
  TEntry e;
  int r, i, l;
  CHAR lbuf[KMaxFileName];

  TPtr fn(_CL(fname, lbuf), strlen(fname), KMaxFileName);
  debug1(2, " Stat |%S| ", &fn);
  if(!isvolume)
    {
      if((r = iFs->Entry(fn, e)))
	return r;

      // Timestamp
#define NUMHIGH 14474675L
#define NUMLOW  254771200L
      TInt64 epoc64(NUMHIGH, NUMLOW);
      TTime tepoc(epoc64);
      TTimeIntervalSeconds isec;
      e.iModified.SecondsFrom(tepoc, isec);
      l = isec.Int();
      icp(rbuf+8, &l);
    }
  
  if(isvolume || (e.iAtt & KEntryAttDir) || (e.iAtt & KEntryAttVolume))
    {
      CDir *cdir = 0;
      rbuf[2] = (1<<4);
      fn.Append('\\');
      r = iFs->GetDir(fn, KEntryAttMatchMask, ESortNone, cdir);
      l = 0;
      for(i = 0; i < cdir->Count(); i++)
        if((*cdir)[i].IsDir())
	  l++;
      delete cdir;
      rbuf[0] = (char)(l & 0xff);
      rbuf[1] = (char)((l>>8) & 0xff);
    }
  else
    {
      rbuf[2] = 0; rbuf[3] = 1;
      if(!(e.iAtt & KEntryAttReadOnly)) rbuf[2] |= (1<<0);
      if(e.iAtt & KEntryAttHidden)      rbuf[2] |= (1<<1);
      if(e.iAtt & KEntryAttSystem)      rbuf[2] |= (1<<2);
      icp(rbuf+4, &e.iSize);
    }
  return 0;
}

static int
WaitAsync(struct global_data *gd)
{
  if(gd->asyncop != 1)
    return KErrNone;
  User::WaitForRequest(gd->asyncst);
  if(gd->asyncst.Int() != KErrNone) // Shit happened
    debug0(1, "A");
  return gd->asyncst.Int();
}

/////////////////////////////////////////////
// Handle a request.
void
DoReq(struct global_data *gd, int len)
{
  char *buf = gd->doreqbuf; // Command & filename, Read buffer
  char buf2[KMaxFileName]; // Stat & getdevs answer; New filename for rename

  CDir *cdir = 0;
  RFile iFile;
  int cmd, r, repchar, answer, i, off;
  unsigned int dlen;
  char zero = 0, one = 1;
  TBool last_block = EFalse;
  RFs *iFs = gd->iFs;

  debug0(2, "\n");
  if(len <= 0)
    return;
  if(DoRead(gd, buf, len))
    return;
  cmd = buf[len-1];

  TPtrC fn(_CL(buf+1, gd->ubuf1));
  //debug2(2, "CMD %d for |%s|: ", cmd, gd->ubuf1);

  if(gd->rfilename[0] && cmd != 4)// Close the read handle, if not reading.
    {
      gd->iRFile->Close();
      gd->rfilename[0] = 0;
    }
  if(gd->wfilename[0] && cmd != 10)// Close the write handle, if not writing.
    {
      gd->iWFile->Close();
      gd->wfilename[0] = 0;
    }

  answer = 1;
  r = 0;

  gd->asyncop = WaitAsync(gd) ? 2 : 0;

  switch(cmd)
    {
      case 1:	// Create
	r = iFile.Replace(*iFs, fn, EFileShareAny);
	if(!r)
	  iFile.Close();
	repchar = r ? 'C' : 'c';
	break;
      case 2:	// GetAttr
        answer = 0;
	r = doStat(gd, buf+1, *buf, buf2, 0);
        DoWrite(gd, r ? &one: &zero, 1);
	if(!r)
	  r = DoWrite(gd, buf2, 12);
        repchar = r ? 'G' : 'g';
	break;
      case 3:	// MkDir
        buf[*buf+1] = '\\'; buf[*buf+2] = 0;
	r = iFs->MkDir(_C(buf+1, gd->ubuf1));
	repchar = r ? 'M' : 'm';
	break;
      case 4:	// Read
	icp(&off, buf+len-7);
	dlen = buf[len-3] + (buf[len-2] << 8);
	debug2(2, "Read at %d, len %d ", off, dlen);

	// Open the file
	if(strcmp(buf+1, gd->rfilename))
	  {
	    if(gd->rfilename[0]) // Close it if another one is open
	      {
	        gd->iRFile->Close();
		gd->rfilename[0] = 0;
	      }
	    if((r = doStat(gd, buf+1, *buf, gd->rbuf, 0)))
	      goto read_done;
	    if((r = gd->iRFile->Open(*iFs, fn,
	    		EFileShareAny|EFileRead|EFileStream)))
	      goto read_done;
	    strcpy(gd->rfilename, buf+1);
	  }

	icp(&i, buf2+4);	// File Size
	if(off+dlen > (unsigned int)i)
	  dlen = i - off;
	answer = 0;
	DoWrite(gd, &zero, 1);
	DoWrite(gd, gd->rbuf, 12);
	while(dlen > 0)
	  {
	    if(gd->asyncoffset == off && gd->asyncop == 0 && dlen == BLOCKSIZE)
	      {
		DoWrite(gd, gd->asyncbuf, dlen);
		off += dlen;
		dlen = 0;
	      }
	    else
	      {
		TPtr8 p((unsigned char *)buf, BLOCKSIZE);
		int l = dlen > BLOCKSIZE ? BLOCKSIZE : dlen;
		gd->iRFile->Read(off, p, l);
		DoWrite(gd, buf, l);
		off += l;
		dlen -= l;
	      }
	  }
	gd->asyncoffset = -1;
	if(i >= off + BLOCKSIZE) // Async read ahead.
	  {
	    TPtr8 p((unsigned char *)gd->asyncbuf, BLOCKSIZE);
	    gd->iRFile->Read(off, p, BLOCKSIZE, gd->asyncst);
	    gd->asyncoffset = off;
	    gd->asyncop = 1;
	  }
read_done:
	repchar = r ? 'R' : 'r';
	break;
      case 5:	// ReadDir
	r = iFs->GetDir(fn, KEntryAttMatchMask, ESortNone,cdir);
	answer = 0;
	DoWrite(gd, r ? &one : &zero, 1);
	if(!r)
	  {
	    for(i = 0; i < cdir->Count(); i++)
	      {
		char *name = _c((*cdir)[i].iName, gd->cbuf);
		DoWrite(gd, name, strlen(name));
		DoWrite(gd, &zero, 1);
	      }
	    DoWrite(gd, &zero, 1);
	    delete(cdir);
	  }
	repchar = r ? 'L' : 'l';
	break;
      case 6:	// Remove
        r = iFs->Delete(fn);
	repchar = r ? 'D' : 'd';
	break;
      case 7:	// Rename
	dlen = buf[len-2];
        r = DoRead(gd, buf2, dlen);
	buf2[dlen] = 0;
	if(!r)
	  {
	    r = iFs->Rename(fn, _C(buf2+1, gd->ubuf2)); // old, new
	    /**
	     * Unix: man 2 rename
	     * If newpath already exists it will be atomically replaced
	     * (subject to a few conditions - see ERRORS below), so that there
	     * is no point at which another process attempting to access
	     * newpath will find it missing.
	     */
	    if(r==KErrAlreadyExists)
	      {
		r = iFs->Delete(_C(buf2+1, gd->ubuf2)); // delete target file
		if(!r)
		  r = iFs->Rename(fn, _C(buf2+1, gd->ubuf2)); // old, new
	      }
	  }
	repchar = r ? 'N' : 'n';
	break;
      case 8:	// RmDir
        buf[*buf+1] = '\\'; buf[*buf+2] = 0;
        r = iFs->RmDir(_C(buf+1, gd->ubuf1));
	repchar = r ? 'Z' : 'z';
	break;
      case 9:	// SetAttr, NYI
	r = 0;
	repchar = 's';
        break;
      case 10:	// Write
	icp(&off, buf+len-7);
	dlen = buf[len-3] + (buf[len-2] << 8);
	debug2(2, "Write at %d, len %d ", off, dlen);

	if(strcmp(buf+1, gd->wfilename))
	  {
	    if(gd->wfilename[0])
	      {
	        gd->iWFile->Close();
		gd->wfilename[0] = 0;
	      }
	    if((r = gd->iWFile->Open(*iFs, fn,
		      EFileShareExclusive|EFileWrite|EFileStream)) == 0)
	      strcpy(gd->wfilename, buf+1);
	  }

	if(gd->asyncop == 2)
	  r = 1;
	gd->asyncop = 0;
	gd->asyncoffset = -1;

	last_block = (dlen < BLOCKSIZE); // used later
	while(dlen)
	  {
	    int l = dlen > BLOCKSIZE ? BLOCKSIZE : dlen;
	    if((r = DoRead(gd, buf, l)))
	      goto write_done;

	    TPtrC8 p((unsigned char *)buf, l);
	    if(!r)
	      {
		r = gd->iWFile->Write(off, p, l);
		debug2(3, " WF(%d)=%d ", l, r);
		off += l;
	      }
	    dlen -= l;
	  }

write_done:
	if(r && gd->wfilename[0])
	  {
	    gd->iWFile->Close();
	    gd->wfilename[0] = 0;
	  }

	if(last_block)
	  {
	    if(gd->wfilename[0])
	      gd->iWFile->Close();
	    gd->wfilename[0] = 0;

	    /** auto-execution of .exe's - last block is *probably* less
	     *  than BLOCKSIZE :) press key 'x' to toggle on/off
	     */
	    if(gd->auto_exec)
	      {
		if(fn.MatchF(_L("*.exe")) != KErrNotFound)
		  {
		    debug1(1, "\nexecuting file '%S'", &fn);
		    RProcess p;
		    p.Create(fn, KNullDesC);
		    p.Resume();
		    p.Close();
		  }

	      }
	  }
	
	repchar = r ? 'W' : 'w';
	break;
      case 11:	// GetDevs
	{
	  TDriveList dl;
	  TVolumeInfo vi;

	  iFs->DriveList(dl);
          DoWrite(gd, &zero, 1);
	  for(i = 0; !r && i < KMaxDrives; i++)
	    {
	      if(!dl[i]) continue;
	      if(iFs->Volume(vi, i)) continue;

	      buf2[0] = (char)(i+'A');
		  buf2[1] = ':';
		  buf2[2] = 0;
	      DoWrite(gd, buf2, 3);
	      buf2[2] =
	      	(char)((vi.iDrive.iMediaAtt==KMediaAttWriteProtected) ? 5 : 0);
	      dlen = vi.iSize.Low(); icp(buf2+ 6, &dlen);
	      dlen = vi.iFree.Low(); icp(buf2+10, &dlen);
	      r = DoWrite(gd, buf2, 14);
	    }
	  repchar = '>';
	}
	break;
      case 12:	// StatDev
	answer = 0;
        r = doStat(gd, buf+1, *buf, buf2, 1);
	DoWrite(gd, r ? &one : &zero, 1);
	if(!r)
	  DoWrite(gd, buf2, 12);
	repchar = r ? 'F' : 'f';
	break;
      default:
	// Oops. Lets read the garbage away. Takes at least 3 seconds!
        while(DoRead(gd, buf2, sizeof(buf2)) != KErrTimedOut)
	  ;
        repchar = 'X'; r = 1;
        break;
    }

  if(answer)
    DoWrite(gd, r ? &one: &zero, 1);
  debug1(1, "%c", repchar);
}

/////////////////////////////////////////////
// Switch to nfsclient, if already runnig
int
switchTo(const TDesC &pgmname)
{
  RWsSession	ws;
  int i, j, x, nw, found = 0;

  TInt r = ws.Connect();
  if(r != KErrNone)
	return r;

  nw = ws.NumWindowGroups();

  CArrayFixFlat<int> *fl = new CArrayFixFlat<int>(5);
  ws.WindowGroupList(fl);
  for(i = 0; !found && i < nw; i++)
    {
      TBuf<KMaxFileName> name1, name2;
      ws.GetWindowGroupNameFromIdentifier(fl->At(i), name1);
      for(x = j = 0; x != 2 && j  < name1.Length(); j++)
        if(name1[j] == 0 )
	  x++;
      if(x == 2)
        {
	  name2.Copy(name1.Mid(j));
	  name2.SetLength(name2.Length()-1);
	  if(pgmname.Compare(name2) == 0)
	    {
	      ws.SetWindowGroupOrdinalPosition(fl->At(i), 0);
	      found = 1;
	    }
        }
    }
  delete fl;
  ws.Close();

  return found ? KErrNone : KErrNotFound;
}


int
SetCommPort(RCommServ &iServer, struct global_data *gd)
{
#ifdef _UNICODE
  CHAR lbuf1[32], lbuf2[32];
#endif
  char lbuf3[32];

  if(gd->iCommPort)
    {
      gd->iCommPort->Cancel();
      delete gd->iCommPort;
    }
  int r;
  cp = cptable + gd->port;

  const char *portname = GetConfig(gd, cp->mod, lbuf3, cp->portname);
  debug2(1, "Open %s/%s\n", _CL(cp->mod, lbuf1), _CL(portname, lbuf2));

#if defined(_BLUETOOTH) || defined(_TCP)
  if(cp->type == T_BLUE || cp->type == T_TCP)
    {
#if defined(_BLUETOOTH)
      if(cp->type == T_BLUE) gd->iCommPort = new RfComm;
#endif
#if defined _TCP
      if(cp->type == T_TCP) gd->iCommPort = new TcpipComm;
#endif

      TRequestStatus st, rs_c;
      TBuf<32> hname;
      TPtrC dest(_CL(portname, lbuf2));
      if((r = gd->iCommPort->Open(gd->cons, hname, dest, st)) == KErrNone)
	{
	  debug0(1, "Type a key to cancel\n");
	  gd->cons->ReadCancel();
	  gd->cons->Read(rs_c);

	  while(st == KRequestPending && rs_c == KRequestPending)
	    User::WaitForAnyRequest();

	  if(st != KErrNone)
	    {
	      gd->iCommPort->Cancel();
	      r = st.Int();
	    }
	  gd->cons->ReadCancel();
	}
    }
  else
#endif
    {
      iServer.LoadCommModule(_C(cp->mod, lbuf1));
      MRComm *mrp = new MRComm;
      gd->iCommPort = mrp;
      mrp->iR = new RComm;
      r = mrp->iR->Open(iServer, _C(portname, lbuf1), ECommExclusive);
    }


  if(r)
    {
      debug1(1, "failed (%d)\n", r);
      delete gd->iCommPort;
      gd->iCommPort = 0;
      return 0;
    }
  debug0(1, "Connected\n");

  if(cp->type == T_SERIAL)
    {
      TCommConfig portSettings;
      RComm *r = ((MRComm *)gd->iCommPort)->iR;
      r->Config(portSettings);
      portSettings().iRate = EBps115200;
      portSettings().iParity = EParityNone;
      portSettings().iDataBits = EData8;
      portSettings().iStopBits = EStop1;
      portSettings().iHandshake = KConfigFailDSR;

      r->SetConfig(portSettings);

      r->SetReceiveBufferLength(4352);	// ?? why exactly 4235?
      debug1(2, "ReceiveBuffer: %d\n", r->ReceiveBufferLength());
    }
  return 1;
}

int
mainL()
{
  RCommServ iServer;
  TRequestStatus rs_c, rs_s;
  _LIT(KPgmName, "Nfsclient");
  int r;

  struct global_data *gd =
  		(struct global_data *)User::Alloc(sizeof(struct global_data));
  Mem::FillZ(gd, sizeof(*gd));
  
  gd->auto_exec = 0;
  gd->iFs = new RFs;
  gd->iRFile = new RFile;
  gd->iWFile = new RFile;
  gd->readbuf.Set(gd->readbufbuf, 0, BLOCKSIZE);
  gd->iTimer = new RTimer;
  gd->iTimer->CreateLocal();
  gd->asyncoffset = -1;

  r = gd->iFs->Connect();
  if(r != KErrNone)
    return r;

  // Search for other Nfsclient Windows, and switch to them if
  // one is running already.
  if(switchTo(KPgmName) == KErrNone)
    return 0;

  gd->cons =
  	//Console::NewL(KPgmName, TSize(KDefaultConsWidth, KDefaultConsHeight));
  	Console::NewL(KPgmName, TSize(KDefaultConsWidth, 10));
  CConsoleBase *cons = gd->cons;
  cons->Printf(_L("Welcome to nfsclient\n"));
  cons->Printf(_L("Ver:" VERSION ", Licence:GPL\n\n"));
#if defined(_UIQ)
  cons->Printf(_L("- Scroll down for switching i/o\n"));
  cons->Printf(_L("  interfaces (Bluetooth/IrDA)\n"));
  cons->Printf(_L("- Press the jogdial to exit\n"));
  cons->Printf(_L("- Scroll up to change debuglevel\n\n"));
#else
#if defined(_SERIES60)
  cons->Printf(_L("- Press joystick for\n"));
  cons->Printf(_L("  switching i/o intf.\n"));
  cons->Printf(_L("- Press right button\n"));
  cons->Printf(_L("  to exit\n\n"));
#endif
  cons->Printf(_L("Type h for help\n\n"));
#endif


  // Read in the config file
  strcpy(gd->rfilename, "C:\\System\\Apps\\nfsapp\\nfsclient.ini");
  gd->config[0] = 0;
  for(int i = 0; i < 10; i++) // Try the drives between C: and L:
    {
      gd->rfilename[0] = 'C'+i;
      if(gd->iRFile->Open(*gd->iFs, _C(gd->rfilename, gd->ubuf1),
		    EFileShareAny|EFileRead|EFileStreamText) == KErrNone)
	{
	  TPtr8 p((unsigned char *)gd->config, CMDBUF);
	  gd->iRFile->Read(p);
	  gd->config[p.Length()] = 0;
	  gd->iRFile->Close();
	  break;
	}
    }
  gd->rfilename[0] = 0;
  char lbuf[32];
  gd->debug = *GetConfig(gd, "Debug", lbuf, "1") - '0';

  // Set the port from the config file
  const char *mod = GetConfig(gd, "Port", lbuf, "");
  for(unsigned int i = 0; i < NPORT; i++)
    if(!strcmp(mod, cptable[i].mod))
      {
        gd->port = i;
	break;
      }


#if defined(_PLP)
  /**
   * try to stop PLP first (which hoggs COMM::0)
   */
  RRemoteLink link;
  r = link.Open();
  if(r == KErrNone)
    {
      debug1(1, "Stopping PLP...", 0);
      r = link.Disable();
      debug1(1, "[ret=%d]\n", r);
      link.Close();
    }
#endif
  


  /**
   * @todo check return values here
   */
  r = User::LoadPhysicalDevice(_L("EUART1")); //debug1(1, "PDD     : %d\n", r);
  r = User::LoadLogicalDevice(_L("ECOMM"));   //debug1(1, "LDD     : %d\n", r);
  r = StartC32();			      //debug1(1, "StartC32: %d\n", r);
  r = iServer.Connect();		      //debug1(1, "Server  : %d\n", r);

  SetCommPort(iServer, gd);
  cons->Read(rs_c);

  TBuf8<1> sb;
  if(gd->iCommPort)
    gd->iCommPort->Read(rs_s, sb);
  else
    rs_s = KRequestPending;


  FOREVER
    {
      User::WaitForAnyRequest();
      
      if(rs_c != KRequestPending)
	{
	  TKeyCode k = cons->KeyCode();
	  switch(k)
	    {
#ifdef _SERIES60
	      case EKeyDevice1:		// Joystick press on the 7650
#endif
#ifdef _UIQ
	      case EKeyDevice8:		//EStdQuartzKeyConfirm
#endif
	      case EKeyEscape:

	      case 'q':
	      case 't':	// 7650
	      case 5:
	      case 10003:
		return 0;
#ifdef _UIQ
	      case EKeyDevice1:		// EQuartzKeyTwoWayUp
	      case 63558:		// Motorola 920 / 925
#endif
	      case 'd':
		gd->debug = (gd->debug+1) % 4;
		cons->Printf(_L("debug=%d\n"), gd->debug);
		break;
	      case 'x':
	      case 'w':			// 7650
		gd->auto_exec = !gd->auto_exec;
		cons->Printf(_L("auto_exec=%d\n"), gd->auto_exec);
		break;
#ifdef _SERIES60
	      case EKeyDevice3:		// Right display button on the 7650
#endif
#ifdef _UIQ
	      case EKeyDevice2:		// EQuartzKeyTwoWayDown
	      case 63559:		// Motorola 920 / 925
#endif
	      case 'p':
		gd->port = (gd->port+1) % NPORT;
		SetCommPort(iServer, gd);
		rs_s = KRequestPending;
		if(gd->iCommPort) gd->iCommPort->Read(rs_s, sb);
	        break;
	      case 'g':			// 7650
	      case 'h':
  		cons->Printf(_L("\nVer:" VERSION ", Licence:GPL\n"));
  		cons->Printf(_L("Keyboard commands:\n"));
		cons->Printf(_L("  q/t (exit)\n"));
		cons->Printf(_L("  d   (debug on/off)\n"));
		cons->Printf(_L("  w/x (exec on/off)\n"));
		cons->Printf(_L("  p   (switch comm.)\n"));
  		cons->Printf(_L("Program output:\n"));
  		cons->Printf(_L("  w   (write one block)\n"));
  		cons->Printf(_L("  r   (read  one block)\n"));
  		cons->Printf(_L("  l   (lookup)\n"));
  		cons->Printf(_L("  c   (create)\n"));
	        break;
	      default:
		cons->Printf(_L("<Kbd:%d>"), k);
	        break;
	    }
	  cons->Read(rs_c);
	}
      if(rs_s != KRequestPending)
	{
	  if(rs_s == KErrAbort)
	    {
	      gd->iCommPort->Cancel();
	    }
	  else if(rs_s != KErrNone)
	    {
	      gd->iCommPort->Cancel();
	      rs_s = KRequestPending;
              debug1(1, "\nConnection closed (%d)\n", rs_s);
	      if(cptable[gd->port].type == T_BLUE)
		{
                  debug1(1, "Opening again\n", rs_s);
		  SetCommPort(iServer, gd);
		  rs_s = KRequestPending;
		  if(gd->iCommPort) gd->iCommPort->Read(rs_s, sb);
		  cons->Read(rs_c);
		}
	      continue;
	    }
	  else
	    {
	      debug1(3, "Got: %x", sb[0]);

	      // characters >= 240 appear when the pc is connected...
	      if(sb.Length() && sb[0] && sb[0] < 240)
		{
		  FOREVER
		    {
		      DoReq(gd, sb[0]);
		      if(gd->gb_idx == gd->readbuf.Length())
			break;
		      sb[0] = (unsigned char)GetByte(gd);
		    }
		}
	    }
	  gd->iCommPort->Read(rs_s, sb);
	}
    }
}

/////////////////////////////////////////////
// The main loop.
GLDEF_C int
E32Main()
{
  CTrapCleanup* cleanup = CTrapCleanup::New(); // get clean-up stack
  TRAPD(error, mainL());
  delete cleanup;
  return 0;
}
