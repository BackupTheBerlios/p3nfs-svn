#ifndef __RFCOMM_H

// Small translator from Rfcomm socets to RComm
#include <in_sock.h>
#include "nfsclient.h"
#include <e32cons.h>

class RfComm : public MComm {
public:  
  ~RfComm();
  
  int Open(CConsoleBase *cons, TDes &hostname, TDesC &destination,
  		TRequestStatus &st);
  void Read(TRequestStatus &aStatus, TDes8 &aDes);
  void ReadOneOrMore(TRequestStatus &aStatus, TDes8 &aDes);
  void Write(TRequestStatus &aStatus, const TDesC8 &aDes);
  void Cancel();

protected:
  RSocketServ socketServ;
  RSocket listen;
  RSocket sock;
  TSockXfrLength iRLen;
};

#endif
