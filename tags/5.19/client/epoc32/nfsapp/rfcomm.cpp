#ifdef _BLUETOOTH
#include "rfcomm.h"

#define OldStyleSecurity
#ifdef OldStyleSecurity
#define __BC70s__
#endif

#include <btmanclient.h>	// Security
#include <bt_sock.h>
#include <btsdp.h>		// Service Advertizing
#include <f32file.h>
#include <e32def.h>		// _asm
#include "util.h"

const TInt KServiceClass = 0x1101;

static void
SetSecurityOnChannel(TBTSockAddr &addr, int aPort)
  
{
// I have to distinguish the old SDK from the new one:
// _asm is only defined in the new one
#if defined(_NEWBT)
#define RBTSecuritySettings RBTSecuritySettingsB
#define TBTServiceSecurity TBTServiceSecurityB
#endif

  RBTMan secManager;

  // a security session
  RBTSecuritySettings secSettingsSession;

  // define the security on this port
  secManager.Connect();
  secSettingsSession.Open(secManager);

  // the security settings 
  TBTServiceSecurity serviceSecurity(TUid::Uid(0x101F7058), KSolBtRFCOMM, 0);

  //Define security requirements
  serviceSecurity.SetAuthentication(EFalse);    
  serviceSecurity.SetEncryption(EFalse); 
  serviceSecurity.SetAuthorisation(ETrue);

  serviceSecurity.SetChannelID(aPort);
  TRequestStatus status;
  secSettingsSession.RegisterService(serviceSecurity, status);
  User::WaitForRequest(status); // wait until the security settings are set
}
    

RfComm::~RfComm()
{
  listen.Close();
  sock.Close();
  socketServ.Close();
}

int
RfComm::Open(CConsoleBase *cons, TDes &name, TDesC &destination,
			TRequestStatus &st)
{
  int r;

  socketServ.Connect();
  if((r = listen.Open(socketServ, _L("RFCOMM"))) != KErrNone)
    return r;

  int aPort;
  TLex parse(destination);
  parse.Val(aPort);

  TBTSockAddr addr;
  addr.SetPort(aPort);

  if((r = listen.Bind(addr)) != KErrNone)
    {

      // Perhaps the port is busy. Try it again with a free port
      listen.GetOpt(KRFCOMMGetAvailableServerChannel, KSolBtRFCOMM, aPort);
      addr.SetPort(aPort);
      if((r = listen.Bind(addr)) != KErrNone)
	return r;
    }
  listen.Listen(4);

  if((r = sock.Open(socketServ)) != KErrNone)
    return r;
  listen.Accept(sock, st);
  SetSecurityOnChannel(addr, aPort);

  return KErrNone;
}


void
RfComm::Read(TRequestStatus &aStatus, TDes8 &aDes)
{
  sock.Read(aDes, aStatus);
}

void
RfComm::ReadOneOrMore(TRequestStatus &aStatus,TDes8 &aDes)
{
  sock.RecvOneOrMore(aDes, 0, aStatus, iRLen);
}

void
RfComm::Write(TRequestStatus &aStatus, const TDesC8 &aDes)
{
  sock.Write(aDes, aStatus);
}

void
RfComm::Cancel()
{
  sock.CancelAll();
}
#endif // _BLUETOOTH
