//
// This file is part of p3nfs
//
#include <nfsapp.h>
#include <eikdll.h>
#include <e32def.h>

EXPORT_C CApaApplication *
NewApplication()
{
  return new CNfsApplication;
}

GLDEF_C int
E32Dll(TDllReason)
{
  return KErrNone;
}

void
CNfsAppUi::ConstructL()
{
  TFileName fnb;
  fnb.Copy(Application()->AppFullName());
  int l;
  for(l = fnb.Length()-1; l; l--)
    if(fnb[l] == '\\')
      break;
  fnb.SetLength(l+1);
  fnb.Append(_L("nfsclient.exe"));
#if !defined(__int64)	// Old SDK
  EikDll::StartExeL(fnb);
#else
 RProcess pr;
 if(pr.Create(fnb,fnb) == KErrNone)
   {
     pr.Resume();
     pr.Close();
   }
#endif

  User::Exit(0);
}
