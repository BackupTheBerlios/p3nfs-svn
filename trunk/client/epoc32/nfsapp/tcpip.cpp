#ifdef _TCP
#include "tcpip.h"


TcpipComm::~TcpipComm()
{
  iSock.Close();
  iServ.Close();
}

int
TcpipComm::Open(CConsoleBase *cons, TDes &name, TDesC &destination,
			TRequestStatus &st)
{
  int r;
  TInetAddr addr;
  TPtrC servername;
  int port = 0;

  if((r = iServ.Connect()) != KErrNone)
    return r;
  if((r = iSock.Open(iServ,KAfInet,KSockStream,KProtocolInetTcp)) != KErrNone)
    return r;


  //////////////
  // Parse the destination, which is of the form ip.adress:port
  for(r = 0; r < destination.Length(); r++)
    if(destination[r] == ':')
      break;
  servername.Set(destination.Left(r)); // Wont include ':'
  TLex parser(destination.Mid(r+1));
  parser.Val(port);

  addr.SetPort(port);
  if(addr.Input(servername) != KErrNone) // Its a real hostname, wont resolv
    return 1;

  iSock.Connect(addr, st);

  TPckgBuf<int> one(1);
  iSock.SetOpt(KSoTcpNoDelay, KSolInetTcp, one);

  return 0;
}

void
TcpipComm::Read(TRequestStatus &aStatus, TDes8 &aDes)
{
  iSock.Read(aDes, aStatus);
}

void
TcpipComm::ReadOneOrMore(TRequestStatus &aStatus,TDes8 &aDes)
{
  iSock.RecvOneOrMore(aDes, 0, aStatus, iRLen);
}

void
TcpipComm::Write(TRequestStatus &aStatus, const TDesC8 &aDes)
{
  iSock.Write(aDes, aStatus);
}

void
TcpipComm::Cancel()
{
  iSock.CancelAll();
}
#endif //_TCPIP
