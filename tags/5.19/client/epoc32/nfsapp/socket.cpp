#include <nifman.h>
#include <eikenv.h>	// CEikonEnv


// And now the main class, which is responsible for timeout handling,
// connection establishing, read & write setup.

CMdTSock *
CMdTSock::NewL(MdTUINotify *aUINotify, MdTResource *aRsc)
{
  CMdTSock *self = new (ELeave) CMdTSock();
  CleanupStack::PushL(self);
  self->ConstructL(aUINotify, aRsc);
  CleanupStack::Pop();
  return self;
}


void
CMdTSock::ConstructL(MdTUINotify *aUINotify, MdTResource *aRsc)
{
  iUINotify = aUINotify;
  iTimeOut = 2 * 60 * 1000000; //  2 min, can be modified
  iRsc = aRsc;
  iSockState = Disconnected;
  iSockOp = NOP;
  iTimer = CMdTTimer::NewL(this);
  CActiveScheduler::Add(this);
  iServConnected = 0;
}

void
CMdTSock::ConnectL(const TPtrC &aServerName, int aPort)
{
  TInetAddr ti;

  if(iSockState != Disconnected)
    return;
  if(!iServConnected)
    {
      PlpStop((CMdTAppUi *)CEikonEnv::Static()->EikAppUi());
      User::LeaveIfError(iServ.Connect());
      iServConnected = 1;
    }
  User::LeaveIfError(iSock.Open(iServ, KAfInet, KSockStream, KProtocolInetTcp));
  iAddr.SetPort(aPort);
  if(ti.Input(aServerName) != KErrNone) // Its a real hostname
    {
      User::LeaveIfError(iResolver.Open(iServ,KAfInet,KProtocolInetUdp));
      iResolver.GetByName(aServerName, iNameEntry, iStatus);
      iSockOp = Lookup;
    }
  else
    {
      iAddr.SetAddress(ti.Address());
      iSock.Connect(iAddr, iStatus);
      iSockOp = Connecting;
    }
  iSockState = InWork;
  SetActive();
  iTimer->After(iTimeOut);
}

void
CMdTSock::Read(TDes8 &buf)
{
  if(iSockState == InWork || iSockState == Disconnected)
    return;
  buf.SetLength(0);
  iSock.RecvOneOrMore(buf, 0, iStatus, iRLen);
  iSockState = InWork;
  iSockOp = Reading;
  iTimer->After(iTimeOut);
  SetActive();
}

void
CMdTSock::Write(const TDesC8 &buf)
{
  if(iSockState == InWork || iSockState == Disconnected)
    return;
  iSock.Write(buf, iStatus);
  iSockState = InWork;
  iSockOp = Writing;
  iTimer->After(iTimeOut);
  SetActive();
}

void
CMdTSock::TimerExpired()
{
  if(iSockState != InWork)
    return;

  Cancel();

  iSockState = TimeOut;
  TRequestStatus* p=&iStatus;
  SetActive();
  User::RequestComplete(p, KErrTimedOut);
}

void
CMdTSock::RunL()
{
  Cancel();
  iTimer->Cancel();

  if(iSockOp == Lookup)
    {
      if(iStatus == KErrNone)
        {
	  iAddr.SetAddress(TInetAddr::Cast(iNameEntry().iAddr).Address());
	  iSock.Connect(iAddr, iStatus);
	  iSockOp = Connecting;
	  iSockState = InWork;
	  iTimer->After(iTimeOut);
	  SetActive();
	  return;
	}
    }
  else if(iSockOp == Reading && iStatus == KErrNone)
    {
      iBytesRead =  *(TInt *)iRLen.Ptr();  // Whose idea was this!?!
    }

  if(iSockState == InWork)
    {
      if(iStatus == KErrNone) {
	iSockState = Done;
      } else if(iStatus == KErrEof) {
	iSockState = Eof;
      } else {
	iSockState = Failed;
      }
    }
  iUINotify->Done();  
}

void
CMdTSock::DoCancel()
{
  if(iSockState != InWork)
    return;
  iTimer->Cancel();
  switch(iSockOp)
    {
      case Lookup:
        iResolver.Cancel();
        iResolver.Close();
	break;
      case Connecting:
	iSock.CancelConnect();
	break;
      case Reading:
	iSock.CancelRead();
	break;
      case Writing:
	iSock.CancelWrite();
	break;
      case NOP:
        break;
    }
  iSockState = Cancelled;
}

void
CMdTSock::Disconnect(TInt aCloseServerFlag)
{
  if(iSockState != Disconnected)
    {
      DoCancel();
      iSock.Close();
    }
  if(aCloseServerFlag && iServConnected)
    {
      iServ.Close();
      iServConnected = 0;
    }
  iSockState = Disconnected;
  iSockOp = NOP;
}

char *
CMdTSock::WhatHappened(char *buf)
{
  sprintf(buf, "%s: %s", OpName[iSockOp], StateName[iSockState]);
  return buf;
}

CMdTSock::~CMdTSock()
{
  Disconnect(1);
  delete iTimer;
  Cancel();
}

void
CMdTSock::Hangup()
{
  RNif nif;
  if(nif.Open() == KErrNone)
    {
      nif.Stop();
      nif.Close();
    } 
}

char *
i2str(TInetAddr &ia, char *buf)
{
  char lbuf[20];
  unsigned int a = ia.Address();
  sprintf(lbuf, "%d.%d.%d.%d", (a>>24)&0xff, (a>>16)&0xff, (a>>8)&0xff, a&0xff);
  sprintf(buf, "%16s", lbuf);
  return buf;
}

const char * const state[] = {
  "Pending", "Up", "Busy", "Down",
};

#endif
