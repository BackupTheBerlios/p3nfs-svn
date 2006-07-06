#ifndef __TCPIP_H
#define __TCPIP_H

// Small translator from tcpip socets to RComm
#include <in_sock.h>
#include "nfsclient.h"
#include <e32cons.h>
#include <in_sock.h>

class TcpipComm : public MComm {
public:  
  ~TcpipComm();

  int Open(CConsoleBase *cons, TDes &name, TDesC &destination,
  		TRequestStatus &st);
  void Read(TRequestStatus &aStatus, TDes8 &aDes);
  void ReadOneOrMore(TRequestStatus &aStatus, TDes8 &aDes);
  void Write(TRequestStatus &aStatus, const TDesC8 &aDes);
  void Cancel();

protected:
  RSocket       iSock;
  RSocketServ	iServ;
  TSockXfrLength iRLen;
};

#endif
