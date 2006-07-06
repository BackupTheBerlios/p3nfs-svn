#include "OSdefs.h"
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#if defined(__SVR4) || defined(__GLIBC__)
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#endif
#include "nfs_prot.h"
#include "mp.h"

static struct device *devices;
struct cache *attrcache;
extern int nfsblock;

/*
  Nfsd returned NFSERR_STALE if the psion wasn't present, but I didn't like
  it because the kernel returns the same when the nfsd itself is absent
 */
#define NO_PSION	NFSERR_NXIO

/* FIXME: Send create attributes */
static struct diropres *
create_it(ca, isdir)
  createargs *ca;
  int isdir;
{
  static struct diropres res;
  p_inode *dirinode;
  char *name;
  fattr *fp;
  p_inode *inode;

  if(debug)
    printf("\tcreate: %s (%#o,%d)\n",ca->where.name,ca->attributes.mode,isdir);
  dirinode = get_num(fh2inode(ca->where.dir.data));
  name = dirinode->name;
  if(debug)
    printf("\tcreate: (in %s)\n", name);

  name = build_path(name, ca->where.name);

  if(sendop(isdir ? PFS_OP_MKDIR : PFS_OP_CREATE, iso2cp(name), (char *)0, 0))
    {
      res.status = psion_alive ? NFSERR_NAMETOOLONG : NO_PSION;
      return &res;
    }

  inode = get_nam(name);
  inode2fh(inode->inode, res.diropres_u.diropres.file.data);

  fp = &res.diropres_u.diropres.attributes;
  bzero((char *)fp, sizeof(fp));
  if(isdir)
    {
      fp->type = NFDIR;
      fp->mode = NFSMODE_DIR | 0700;
      fp->nlink = 2;
    }
  else
    {
      fp->type = NFREG;
      fp->mode = NFSMODE_REG | 0600;
      fp->nlink = 1;
    }

  fp->uid = root_fattr.uid;
  fp->gid = root_fattr.gid;
  fp->blocksize = BLOCKSIZE;
  fp->fileid = inode->inode;
  fp->atime.seconds = fp->mtime.seconds = fp->ctime.seconds = time((time_t *)0);
  
  res.status = NFS_OK;

  rem_cache(&attrcache, dirinode->inode);
  rem_cache(&attrcache, inode->inode);
  add_cache(&attrcache, inode->inode, fp);
  return &res;
}

struct diropres *
nfsproc_create_2(ca)
  createargs *ca;
{
  return create_it(ca, 0);
}

struct diropres *
nfsproc_mkdir_2(ca)
  createargs *ca;
{
  return create_it(ca, 1);
}

static void
pattr2attr(op, fp, fh)
  unsigned char *op, *fh;
  fattr *fp;
{
  bzero((char *)fp, sizeof(*fp));

  if(op[2] & (1<<4))
    {
      fp->type   = NFDIR;
      fp->mode   = NFSMODE_DIR | 0700;
/* Damned filesystem.
   We have to count the number of subdirectories on the psion. */
      fp->nlink  = (op[0] | (op[1] << 8)) + 2;
      fp->size   = BLOCKSIZE;
      fp->blocks = 1;
    }
  else
    {
      fp->type   = NFREG;
      fp->mode   = NFSMODE_REG;
      fp->nlink  = 1;
      fp->size   = pstr2long(op+4);
      fp->blocks = (fp->size+BLOCKSIZE-1)/BLOCKSIZE;

/* Following flags have to be set in order to let backups work properly */
      if(op[3] & (1<<0))    fp->mode |= 0400; /* File readable (?) */
      if(op[2] & (1<<0))    fp->mode |= 0200; /* File writeable  */
      if(op[3] & (1<<1))    fp->mode |= 0100; /* File executable */
      if(!(op[2] & (1<<1))) fp->mode |= 0004; /* Not Hidden  <-> world read */
      if(op[2] & (1<<2))    fp->mode |= 0002; /* System      <-> world write */
      if(op[2] & (1<<3))    fp->mode |= 0001; /* Volume      <-> world exec */
      if(op[2] & (1<<5))    fp->mode |= 0020; /* Modified    <-> group write */
      if(op[3] & (1<<2))    fp->mode |= 0040; /* Byte        <-> group read */
      if(op[3] & (1<<3))    fp->mode |= 0010; /* Text        <-> group exec */
    }

  fp->uid = root_fattr.uid; fp->gid = root_fattr.gid;
  fp->blocksize = BLOCKSIZE;
  fp->fileid = fh2inode((char *)fh);
  fp->rdev = fp->fsid = FID;
  fp->atime.seconds = pstr2long(op+8) - gmtoffset;
  fp->mtime.seconds = fp->ctime.seconds = fp->atime.seconds;
}

static int
query_devices()
{
  struct device *dp, *np, **pp;
  char *sp, *p, namebuf[256];
  int link_count = 2; /* set the root link count */

  if(query_cache)
    return 0;
  query_cache = 1;
  for(dp = devices; dp; dp = np)
    {
      np = dp->next;
      free(dp->name); free(dp);
    }
  devices = 0; *namebuf = 0;
  if(sendop(PFS_OP_GETDEVS, namebuf, 0, 0))
    return 1;
  for(pp = &devices; ; pp = &(*pp)->next)
    {
      if(getstr(namebuf))
        return 1;
      if(!*namebuf)
        break;
      link_count++;
      dp = (struct device *)malloc(sizeof(struct device));
      *pp = dp;
      dp->next = 0;
      if(series5)
        sp = namebuf;
      else
	{
	  for(p = sp = cp2iso(namebuf); *p; p++)
	    if (isupper(*p))	/* norm@city.ac.uk, SunOS 4.0.2 */
	      *p = tolower(*p);
	    /* Convert ISO8859-1 */
	    else if(*(unsigned char *)p >= 0xc0 && *(unsigned char *)p <= 0xde) 
	      *p += 0x20;
	}
      dp->name = (char *)strdup((char *)sp);

      if(getcount(dp->attrib, 14))
        return 1;
      dp->total = pstr2long((*pp)->attrib+6);
      dp->free  = pstr2long((*pp)->attrib+10);
    }
  root_fattr.nlink = link_count;
  return 0;
}

struct attrstat *
nfsproc_getattr_2(fh)
  struct mynfs_fh *fh;
{
  static struct attrstat res;
  p_inode *inode;
  fattr *fp = &res.attrstat_u.attributes;
  unsigned char op[12];
  struct cache *cp;
  int l;

  if(debug > 2) printf("\tgetattr\n");
  inode = get_num(fh2inode(fh->data));
  if(debug) printf("\tgetattr:'%s',%d\n", inode->name, inode->inode);
  res.status = NFS_OK;

  if ((cp = search_cache(attrcache, inode->inode)))
    {
      if(debug) printf("\t\tgetattr: cache hit\n");
      *fp = cp->attr;	/* gotcha, all done */
      return &res;
    }

  l = strlen(inode->name);

  if(inode->inode == root_fattr.fileid)
    {
      /* It's the root inode */
      if(debug) printf("\t\tgetattr:root inode (%#o)\n", root_fattr.mode);
  
      if(query_devices()) /* root inode is always there */
        root_fattr.nlink = 2;
      *fp = root_fattr;
    }
  else if( ( series5 && l == 2 && inode->name[1] == ':' ) ||
          ( !series5 && l > 4 && inode->name[3] == ':' &&
               inode->name[4] == ':' &&
               (l == 5 || (l == 7 && inode->name[6] == ':'))) )
    {
      /* It's a device */
      if(debug) printf("\tgetattr:device\n");
      res.status = NO_PSION;
      if(!query_devices())
        {
	  struct device *dp;

	  for(dp = devices; dp; dp = dp->next)
	    if(!strcmp(dp->name, inode->name))
	      break;
	  if(dp)
	    {
	      res.status = NFS_OK;
	      *fp = root_fattr;
	      /* If it's writeable... */
	      if(!(dp->attrib[2] == 5 || dp->attrib[2] == 6))
	        fp->mode |= 0200; 
	      fp->fileid = inode->inode;
	      /* FIXME */
	      if(!strcmp(inode->name, "rom::"))
		fp->nlink = 2;
	      else
	        {
		  if(sendop(PFS_OP_STATDEV, iso2cp(inode->name), 0, 0) ||
		     getcount(op, 12))
		    {
		      res.status = psion_alive ? NFSERR_NOENT : NO_PSION;
		      return &res;
		    }
		  fp->nlink  = (op[0] | (op[1] << 8)) + 2;
		}
	    }
	}
    }
  else
    {
      /* It's a normal file/dir */

      if(sendop(PFS_OP_GETATTR, iso2cp(inode->name), 0, 0) || getcount(op, 12))
	{
	  res.status = psion_alive ? NFSERR_NOENT : NO_PSION;
	  return &res;
	}
      pattr2attr(op, fp, (unsigned char *)fh->data);
    }

  add_cache(&attrcache, inode->inode, fp);

  return &res;
}

struct diropres *
nfsproc_lookup_2(da)
  diropargs *da;
{
  static struct diropres res;
  struct attrstat *gres;
  p_inode *inode;
  char *fp = res.diropres_u.diropres.file.data;

  if(debug > 2) printf("lookup\n");
  inode = get_num(fh2inode(da->dir.data));
  if(!inode)
    {
      if(debug) printf("lookup: stale fh\n");
      res.status = NO_PSION;
      return &res;
    }

  if(debug)
    printf("\tlookup: in '%s'(%d) searching '%s'\n",
           inode->name, inode->inode, da->name);

  if(inode->inode == root_fattr.fileid && !strcmp(da->name, "exit"))
    {
      exiting = 5; /* Lets try it 5 times (10 sec) */
      res.status = NFSERR_EXIST;
      return &res;
    }
  if(inode->inode == root_fattr.fileid && !strcmp(da->name, "debug"))
    {
      debug = (debug+1) & 3;  /* debug level of 0,1,2 & 3 */
      printf("Set debug level to %d\n", debug);
      res.status = NFSERR_EXIST;
      return &res;
    }

  if(!strcmp(da->name, "."))
    inode2fh(fh2inode(da->dir.data), fp);
  else if(!strcmp(da->name, ".."))
    inode2fh(getpinode(inode), fp);
  else if(!old_nfsc && !strncmp(da->name, "exec ", 5))
    {
      char mbuf[1024];

      if(!strcmp(inode->name, "rom::"))
	sprintf(mbuf, "%s%s", inode->name, da->name+5);
      else
	sprintf(mbuf, "%s\\%s", inode->name, da->name+5);
      if(debug)
        printf("\tPSION EXEC: %s\n", mbuf);
      sendop(PFS_OP_EXEC, iso2cp(mbuf), (char *)0, 0);
      res.status = NO_PSION;
      return &res;
    }
  else if(!old_nfsc && !strncmp(da->name, "echo ", 5))
    {
      if(debug)
        printf("\tPSION ECHO: %s\n", da->name+5);
      sendop(PFS_OP_ECHO, iso2cp(da->name+5), (char *)0, 0);
      res.status = NO_PSION;
      return &res;
    }
  else
    inode2fh(get_nam(build_path(inode->name, da->name))->inode, fp);

  gres = nfsproc_getattr_2((struct mynfs_fh *)fp);

  res.status = gres->status;
  res.diropres_u.diropres.attributes = gres->attrstat_u.attributes;
  return &res;
}

#define RA_MAXCOUNT ~0

static void
addentry(ra, where, searchinode, inode, name)
  readdirargs *ra;
  entry ***where;
  int inode, *searchinode;
  char *name;
{
  int l, rndup;

  if(*searchinode)
    {
      if (*searchinode == inode)
        *searchinode = 0;
      return;
    }
  if (ra->count == RA_MAXCOUNT)
    return;

  /* 
   * From ddan@au.stratus.com Wed Feb  8 04:14 MET 1995
   * Modifed in pl7.a by Dan Danz to fix problem of missing files in readdir 
   * 
   * In a shotgun attempt at fixing this, I surmised that perhaps addentry
   * was putting one too many entries in the result, so I increased
   * the number of bytes for each entry by +8  ... my reasoning was that
   * you need to account for the filename pointer and for the cookie int ...
   * but in retrospect, I'm not sure about this reasoning.  HOWEVER, it appears
   * that this fixes the problem.
   * FIXED: See next comment (Rudi)
   */

  /*
   * Count the bytes needed for the xdr encoded data. Xdr is trickier than 
   * one (me :-) might think: Xdr converts a string into 
   * length (4 bytes) + data (rounded up to 4 bytes).
   */
#define XDR_UNIT 4
  l = strlen(name);
  if((rndup = l % XDR_UNIT) > 0)
    l += XDR_UNIT-rndup;
  l += XDR_UNIT; /* Length of name */
  l += sizeof(entry);

  if(l > ra->count)
    {
      ra->count = RA_MAXCOUNT;
      return;
    }
  ra->count -= l;

  **where = (entry *)malloc(sizeof(entry));
  (**where)->fileid = inode;
  (**where)->name = (char *)strdup(name);
  *(int *)(**where)->cookie = inode;
  (**where)->nextentry = 0;
  *where = &(**where)->nextentry;

}

struct readdirres *
nfsproc_readdir_2(ra)
  readdirargs *ra;
{
  static readdirres res;
  p_inode *inode;
  entry *centry, *fentry, **cp;
  char *sp, *p, *bp, buf[256];
  int searchinode;

  if(debug > 2) printf("readdir\n");
  inode = get_num(fh2inode(ra->dir.data));
  if(!inode)
    {
      if(debug) printf("readdir: stale fh\n");
      res.status = NO_PSION;
      return &res;
    }
  cp = &res.readdirres_u.reply.entries;
  for(fentry = *cp; fentry; fentry = centry)
    {
      centry = fentry->nextentry;
      free(fentry->name);
      free(fentry);
    }
  *cp = 0; searchinode = *(int *)ra->cookie;

  if(debug)
    printf("\treaddir: %s, cookie:%x, count:%d\n",
	   inode->name, searchinode, ra->count);

/* . & .. */
  if(debug > 1)
    printf("\tADDENTRY:|%d/%d|\n", inode->inode,getpinode(inode));
  addentry(ra, &cp, &searchinode, inode->inode, ".");
  addentry(ra, &cp, &searchinode, getpinode(inode), "..");

  if(inode->inode == root_fattr.fileid) /* Root directory */
    {
      struct device *dp;

      if(query_devices())
	{
	  res.status = NO_PSION;
	  return &res;
	}
      for(dp = devices; dp; dp = dp->next)
	addentry(ra, &cp, &searchinode, get_nam(dp->name)->inode, dp->name);
    }
  else
    {
      if(sendop(PFS_OP_READDIR, iso2cp(dirname(inode->name)), 0, 0))
	{
	  res.status = psion_alive ? NFSERR_NOENT : NO_PSION;
	  return &res;
	}
      for(;;)
	{
	  if(getstr(buf))
	    {
	      res.status = NO_PSION;
	      return &res;
	    }
	  if(!*buf) break;
	  if(series5)
	    sp = buf;
	  else
	    {
	      for(sp = p = cp2iso(buf); *p; p++)
		/* Convert ISO8859-1 */
		if(*(unsigned char *)p >= 0xc0 && *(unsigned char *)p <= 0xde)
		  *p += 0x20;
		else if (isupper(*p))	/* norm@city.ac.uk, SunOS 4.0.2 */
		  *p = tolower(*p);
	    }

	  bp = filname(sp);
	  addentry(ra, &cp, &searchinode,
		   get_nam(build_path(inode->name, bp))->inode, (char *)bp);
	}
    }

  res.readdirres_u.reply.eof = ra->count == RA_MAXCOUNT ? 0 : 1;
  res.status = NFS_OK;
  return &res;
}

/* FIXME: mode & time not checked */
struct attrstat *
nfsproc_setattr_2(sa)
  sattrargs *sa;
{
  static struct attrstat res;
  p_inode *inode;
  fattr *fp;

  if(debug > 2) printf("setattr\n");
  inode = get_num(fh2inode(sa->file.data));
  if(!inode)
    {
      if(debug) printf("setattr: stale fh\n");
      res.status = NO_PSION;
      return &res;
    }
  if(debug) printf("\tsetattr %s called\n", inode->name);
  res = *nfsproc_getattr_2(&sa->file);
  if(res.status != NFS_OK)
    return &res;
  fp = &res.attrstat_u.attributes;

  if(sa->attributes.size != -1 &&
     sa->attributes.size != fp->size &&
     fp->type == NFREG)
    {
      if(debug)
        printf("\t\tsetattr truncating to %d bytes\n", sa->attributes.size);
      if(sa->attributes.size != 0)
        {
	  res.status = NFSERR_FBIG;
	  return &res;
	}
      if(sendop(PFS_OP_CREATE, iso2cp(inode->name), (char *)0, 0))
	{
	  res.status = psion_alive ? NFSERR_ROFS : NO_PSION;
	  return &res;
	}
      fp->size = 0;
      rem_cache(&attrcache, inode->inode);
      add_cache(&attrcache, inode->inode, fp);
    }
  return &res;
}

static nfsstat *
remove_it(da, isdir)
  diropargs *da;
  int isdir;
{
  static nfsstat res;
  p_inode *inode;
  
  if(debug > 2) printf("remove_it\n");
  inode = get_num(fh2inode(da->dir.data));
  if(!inode)
    {
      if(debug) printf("remove_it: stale fh\n");
      res = NO_PSION;
      return &res;
    }
  if(debug) printf("\tremove_it: in %s: %s (%d)\n",inode->name,da->name,isdir);

  if(sendop(isdir ? PFS_OP_RMDIR : PFS_OP_REMOVE,
            iso2cp(build_path(inode->name, da->name)), 0, 0))
    {
      res = psion_alive ? NFSERR_ACCES : NO_PSION;
      return &res;
    }
  rem_cache(&attrcache, inode->inode);
  res = NFS_OK;
  return &res;
}

nfsstat *
nfsproc_remove_2(da)
  diropargs *da;
{
  return remove_it(da, 0);
}

nfsstat *
nfsproc_rmdir_2(da)
  diropargs *da;
{
  return remove_it(da, 1);
}

nfsstat *
nfsproc_rename_2(ra)
  renameargs *ra;
{
  static nfsstat res;
  p_inode *from, *to;
  char ldata[300], *old, c;

  if(debug > 2) printf("rename\n");
  from = get_num(fh2inode(ra->from.dir.data)),
  to   = get_num(fh2inode(ra->to.dir.data));
  if(!from || !to)
    {
      if(debug) printf("rename: stale fh\n");
      res = NO_PSION;
      return &res;
    }
  strcpy(ldata+1, iso2cp(build_path(to->name, ra->to.name)));
  *ldata = strlen(ldata+1);
  c = *ldata+1;
  old=iso2cp(build_path(from->name, ra->from.name));

  if(debug) printf("\tRename: %s -> %s\n", old, ldata+1);
  if(sendcmd(PFS_OP_RENAME, old, &c, 1) ||
     senddata(ldata, c))
    {
      res = NO_PSION;
      return &res;
    }
  res = getanswer() ?  NFSERR_ACCES : NFS_OK;
  if(res == NFS_OK)
    {
      /* Preserve inode */
      strcpy((char *)ldata, build_path(to->name, ra->to.name));
      (void)re_nam(build_path(from->name, ra->from.name), ldata);
    }

  rem_cache(&attrcache, from->inode);
  rem_cache(&attrcache, to->inode);
  return &res;
}

/* ARGSUSED */
struct statfsres *
nfsproc_statfs_2(fh)
  struct mynfs_fh *fh;
{
  static statfsres res;
  statfsokres *rp;
  struct device *dp;

  if(debug) printf("\tstatfs..\n");

  rp = &res.statfsres_u.reply;
  rp->tsize = nfsblock;
  rp->bsize = BLOCKSIZE;
  rp->blocks = rp->bfree = 0;
  res.status = NFS_OK;

  if(query_devices())
    {
      /* Allow to mount it whithout the psion attached */
      if(psion_alive)
        return &res; /* res.status = NO_PSION;  Hmm */
    }
  for(dp = devices; dp; dp = dp->next)
    {
      rp->blocks += (dp->total + BLOCKSIZE-1)/BLOCKSIZE;
      rp->bfree += (dp->free + BLOCKSIZE-1)/BLOCKSIZE;
    }
  rp->bavail = rp->bfree;

  return &res;
}

/*
 * Problem:
 * Since we are slow (Max 2Kbyte/s) and the biods are very impatient,
 * we receive each request (number of biods + 1 for the kernel itself)
 * times, this number can be lower for the last block. :-(
 * Solution: (not the best, probably)
 * Cache the read data. This cache will be invalidated if there are
 * no more requests in the queue for at least XXX seconds.
 * See also write :-(
 *
 *
 * Another problem: since linux appears to drop bytes (no matter if using
 * HW-flowcontrol or not), we retry our read request...
 */

struct readres *
nfsproc_read_2(ra)
  struct readargs *ra;
{
  static int read_retries = 0;
  static struct readres res;
  static unsigned char rop[NFS_MAXDATA];
  p_inode *inode;
  fattr *fp;
  struct cache *cp;
  struct dcache *dcp;
  int len;
  unsigned int  i_crc;
  unsigned char c_crc[2];

  if(debug > 2) printf("read\n");
  inode = get_num(fh2inode(ra->file.data));
  if(!inode)
    {
      if(debug) printf("read: stale fh\n");
      res.status = NO_PSION;
      return &res;
    }

  if(debug)
    printf("\tread: %s off:%d count:%d\n", inode->name, ra->offset, ra->count);

  cp = search_cache(attrcache, inode->inode);
  if(cp && (dcp = search_dcache(cp, ra->offset, ra->count)))
    {
      if(debug) printf("\tread: cache hit\n");
      res.readres_u.reply.attributes = cp->attr;
      bcopy(dcp->data, res.readres_u.reply.data.data_val, ra->count);
      res.readres_u.reply.data.data_len = ra->count;

      res.status = NFS_OK;
      return &res;
    }

  long2pstr(ra->offset, rop);
  short2pstr(ra->count, rop+4);
  if(sendop(PFS_OP_READ, iso2cp(inode->name), (char *)rop, 6) ||
     getcount(rop, 12))
    {
      res.status = psion_alive ? NFSERR_NOENT : NO_PSION;
      return &res;
    }

  fp = &res.readres_u.reply.attributes;
  pattr2attr(rop, fp, (unsigned char *)ra->file.data);
  if (cp == 0)
    cp = add_cache(&attrcache, inode->inode, fp);

  len = fp->size - ra->offset;
  if(len > ra->count) len = ra->count;
  if(fp->size < ra->offset) len = 0;
  if (debug > 1)
     printf("Read: filesize %d read %d @ %d\n", fp->size, len,ra->offset);
  res.readres_u.reply.data.data_len = len;
  res.readres_u.reply.data.data_val = (char *)rop;

  if(getcount(rop, len))
    {
      if(read_retries++ < 2)
        {
	  if(debug)
	    printf("Read: bad data received, retrying (%d)\n", read_retries);
          return nfsproc_read_2(ra);
	}
      read_retries = 0;
      res.status = NO_PSION;
      return &res;
    }
  read_retries = 0;

  if(!old_nfsc)
    {
      if(getcount(c_crc, 2)) /* Get CRC */
	{
	  res.status = NO_PSION;
	  return &res;
	}
      i_crc = c_crc[0] | c_crc[1] << 8;
      if(debug) printf("CRC: got %#x, computed %#x\n",i_crc,docrc16(rop, len));
      if(i_crc != docrc16(rop, len))
	{
	  printf("p3nfsd: CRC error.\n");
	  res.status = NO_PSION;
	  return &res;
	}
    }

  if (len)
    {
      dcp = add_dcache(cp, ra->offset, ra->count, rop);
      dcp->towrite = 0;		/* don't write it back */
    }

  res.status = NFS_OK;
  return &res;
}


/*
 * Returns cachepointer on full hit, 0 on partial or no hit,
 * see below solaris comment
 */

static int
addwritecache(cp, doff, dlen, dp)
  struct cache *cp;
  int      doff, dlen;
  unsigned char *dp;
{
  struct dcache *dcp;
  int len, changed, os, oe;
  unsigned char *pd, *pc;

  /* 
   * do the cachesearch: we are interested in partial hits, as we don't
   * want to write anything twice (flash ram)
   */
  for(dcp = cp->dcache; dcp; dcp = dcp->next)
    if (doff < dcp->offset + dcp->len && doff + dlen > dcp->offset)
      break;

  if(!dcp)
    {
      /* phew, nothing fancy to do */
      add_dcache(cp, doff, dlen, dp);
      return 0;
    }
  
  os = doff > dcp->offset ? doff : dcp->offset;
  oe = doff + dlen < dcp->offset + dcp->len ? doff + dlen : dcp->offset + dcp->len;
  pd = dp + os - doff;
  pc = dcp->data + os - dcp->offset;
  len = oe - os;

  changed = 0;
  if(bcmp(pd, pc, len))
    {
      bcopy(pd, pc, len);
      dcp->towrite = 1;
      changed = 1;
    }

  if(doff >= dcp->offset && doff + dlen <= dcp->offset + dcp->len)
    {
      if(debug) printf("\twrite: full cache hit\n");
      return !changed;
    }

  if(debug)
    printf("\twrite: partial cache hit (off %d len %d)\n", dcp->offset, dcp->len);
  
  /* Do we have some data below the cached area... */
  if(doff < dcp->offset)
    (void)addwritecache(cp, doff, dcp->offset - doff, dp);

  /* ...or some above? */
  len = (doff + dlen) - (dcp->offset + dcp->len);
  if(len > 0)
    (void)addwritecache(cp, dcp->offset + dcp->len, len, dp + dlen - len);

  return 0;
}



/*
 * The same problem here as above: we receive numerous requests,
 * not even in a specified order. The only good thing is that each request
 * up to the last is exactly the same size.
 * A new problem:
 * Since I dunno how to seek to a point beyond the end of the file (psion),
 * I can't sopport the exact semantics of write. Such files will never be 
 * written completely. :-(
 *
 * Another dumb solaris (sysv?) problem: if the client is writing 512 byte
 * blocks we receive following write requests:
 * off 0 len 512, off 0 len 1024, off 0 len 1536, ... so on till len 4096,
 * that means 4 times as much data as actually needed.
 * We should check if the block was partially written, and write only the
 * difference
 */
struct attrstat *
nfsproc_write_2(wa)
  writeargs *wa;
{
  static struct attrstat res;
  p_inode *inode;
  struct cache *cp;
  struct dcache *dcp;
  fattr *fp;
  unsigned char wop[12], c_crc[2];
  struct attrstat *gres;
  int c,len, dlen, doff;

  inode = get_num(fh2inode(wa->file.data));
  if(debug > 2) printf("write\n");
  if(!inode)
    {
      if(debug) printf("write: stale fh\n");
      res.status = NO_PSION;
      return &res;
    }
  if(debug)
    printf("\twrite:%s off:%d l:%d\n",inode->name,wa->offset,wa->data.data_len);

  dlen = wa->data.data_len;
  doff = wa->offset;

  /* fetch attributes */
  if ((cp = search_cache(attrcache, inode->inode)) == 0)
    {
      gres = nfsproc_getattr_2((struct mynfs_fh *)wa->file.data);
      if(gres->status != NFS_OK)
        {
	  res.status = gres->status;
	  return &res;
	}
      cp = search_cache(attrcache, inode->inode);
      if (!cp) abort();
    }

  fp = &cp->attr;
  if(fp->size < doff + dlen)
    fp->size = doff + dlen;
  fp->blocks = (fp->size + (BLOCKSIZE - 1))/ BLOCKSIZE;
  fp->atime.seconds = fp->mtime.seconds = fp->ctime.seconds = time(0);

  res.attrstat_u.attributes = *fp;

  if(addwritecache(cp, doff, dlen, (unsigned char *)wa->data.data_val))
    {
      res.status = NFS_OK;
      return &res;
    }

/* Write out as many blocks from the cache as we can */
  for(;;)
    {
      if(debug > 2)
	for(dcp = cp->dcache; dcp; dcp = dcp->next)
	  printf("\t\tCheck: %d=%d,%d,%d>=%d\n",
		   inode->inode, cp->inode, dcp->towrite,
		   cp->actual_size, dcp->offset);
      for(dcp = cp->dcache; dcp; dcp = dcp->next)
	if(dcp->towrite && cp->actual_size >= dcp->offset)
	  break;
      if(!dcp) /* Can't write any blocks */
        break;
  
      if(debug)
        printf("\twriting off: %d, len: %d, act: %d\n",
					dcp->offset, dcp->len, cp->actual_size);
      long2pstr(dcp->offset, wop);
      short2pstr(dcp->len, wop+4);

      if(sendcmd(PFS_OP_WRITE, iso2cp(inode->name), (char *)wop, 6) || 
	 senddata((char *)dcp->data, dcp->len))
	{
	  if(debug) printf("write: dump failed\n");
	  res.status = psion_alive ? NFSERR_NOSPC : NO_PSION;
	  return &res;
	}

      if(!old_nfsc)
        {
	  short2pstr(docrc16(dcp->data, dcp->len), c_crc);
	  senddata((char *)c_crc, 2);
        }

      if((c = getanswer()) != 0)
	{
	  if(debug) printf("Psion write error %d.\n", c);
	  res.status = NO_PSION;
	  return &res;
	}
      dcp->towrite = 0;
      len = dcp->offset + dcp->len;
      if(len > cp->actual_size)
        cp->actual_size = len;
      if(debug)
        printf("\twritten: new length: %d\n", cp->actual_size);
    }
    
  if(debug) 
    printf("\twrite -> ISOK (%d, %d %d)\n",
      res.attrstat_u.attributes.size,
      res.attrstat_u.attributes.fileid,
      res.attrstat_u.attributes.fsid);
  res.status = NFS_OK;
  return &res;
}

/* Dummy routines */
void *
nfsproc_writecache_2()
{
  static char res;
  if(debug) printf("writecache???\n");
  res = (char)NFSERR_FBIG;
  return (void *)&res;
}

void *
nfsproc_null_2()
{
  static char res;
  if(debug) printf("null.\n");
  res = (char)NFSERR_FBIG;
  return (void *)&res;
}

void *
nfsproc_root_2()
{
  static char res;
  if(debug) printf("root????\n");
  res = (char)NFSERR_FBIG;
  return (void *)&res;
}

/* not possible operations */
/* ARGSUSED */
nfsstat *
nfsproc_link_2(la)
  linkargs *la;
{
  static nfsstat res;

  if(debug) printf("link..\n");
  res = NFSERR_ACCES;
  return &res;
}


/* ARGSUSED */
struct readlinkres *
nfsproc_readlink_2(fh)
  struct mynfs_fh *fh;
{
  static readlinkres res;

  if(debug) printf("readlink...\n");
  res.status = NFSERR_ACCES;
  return &res;
}

/* ARGSUSED */
nfsstat *
nfsproc_symlink_2(sa)
  symlinkargs *sa;
{
  static nfsstat res;

  if(debug) printf("symlink..\n");
  res = NFSERR_ACCES;
  return &res;
}
