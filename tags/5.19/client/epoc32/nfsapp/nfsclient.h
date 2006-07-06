#ifndef __NFSCLIENT_H
#define __NFSCLIENT_H

class MComm {
public:  
  virtual ~MComm() {}

  virtual int  Open(CConsoleBase *cons, TDes &hostname, TDesC &destination,
  			TRequestStatus &st) = 0;
  virtual void Read(TRequestStatus &aStatus, TDes8 &aDes) = 0;
  virtual void ReadOneOrMore(TRequestStatus &aStatus, TDes8 &aDes) = 0;
  virtual void Write(TRequestStatus &aStatus, const TDesC8 &aDes) = 0;
  virtual void Cancel() = 0;
};

#endif
