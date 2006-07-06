#ifndef __MEDISOCK_H
#define __MEDISOCK_H

#include "config/config.h"

#ifdef CONFIG_SYNC

#include <e32std.h>
#include <e32cons.h>
#include <in_sock.h>
#include <stdio.h>

#include "medidef.h"
class MdTResource;

class MdTTONotify
{
public:
  virtual void TimerExpired() = 0;
};

class MdTUINotify
{
public:
  virtual void Done() = 0;
};


class CMdTTimer : public CTimer
{
public:
  static CMdTTimer *NewL(MdTTONotify *aTONotify);
  ~CMdTTimer() {Cancel(); }
protected:
  CMdTTimer() : CTimer(EPriorityHigh) {}
  void ConstructL(MdTTONotify *aTONotify);
  virtual void RunL();
private:
  MdTTONotify *iTONotify;
};


class CMdTSock : public CActive,
		 public MdTTONotify
{
public:
  enum TOp	{ NOP, Connecting, Reading, Writing, Lookup };
  enum TState	{ Disconnected, InWork, Failed, Done, TimeOut, Cancelled, Eof };


  static CMdTSock *NewL(MdTUINotify *aUINotify, MdTResource *aRsc);
  ~CMdTSock();
  void ConnectL(const TPtrC &aServerName, int aPort);
  void Read(TDes8 &buf);
  void Write(const TDesC8 &buf);
  void Disconnect(TInt aServerCloseFlag);
  char *WhatHappened(char *buf);
  static void Hangup();

  TState iSockState;
  TOp    iSockOp;
  TInt   iBytesRead;
  TInt   iServConnected;
  TInt	 iTimeOut;

protected:
  void ConstructL(MdTUINotify *aUINotify, MdTResource *aRsc);
  void TimerExpired();		// Timer interface

private:
  CMdTSock::CMdTSock() : CActive(EPriorityStandard) {}
  void NextStep();
  void RunL();
  void DoCancel();

  RSocket	iSock;
  RSocketServ	iServ;
  RHostResolver	iResolver;
  TNameEntry	iNameEntry;

  TInetAddr	iAddr;
  CMdTTimer	*iTimer;
  MdTUINotify	*iUINotify;
  TSockXfrLength iRLen;
  MdTResource	*iRsc;
};

#endif // Config Sync

#endif
