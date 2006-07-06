#ifndef _NFSCLIENT_H
#define _NFSCLIENT_H


#include <eikappui.h>
#include <eikapp.h>
#include <eikdoc.h>

class CNfsAppUi : public CEikAppUi
{
public:
  void ConstructL();
  void HandleCommandL(int /* aCommand */) {};
};


///////////////////////////////////////////////////////////////////////
//
// CNfsDocument
// CNfsApplication
// Obviously I do not use these classes the way they are intended to.
//

class CNfsDocument : public CEikDocument
{
public:
  CNfsDocument(CEikApplication& aApp) : CEikDocument(aApp) { }
  CEikAppUi* CreateAppUiL() { return new(ELeave) CNfsAppUi; }
};

class CNfsApplication : public CEikApplication
{
public:
    CApaDocument* CreateDocumentL() { return new (ELeave)CNfsDocument(*this); }
    TUid AppDllUid() const { TUid id = { UID3 };  return id; }
};

#endif
