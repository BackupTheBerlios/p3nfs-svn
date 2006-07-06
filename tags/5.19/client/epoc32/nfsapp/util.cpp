#ifdef _UTIL
#include <f32file.h>
#include <libc/string.h>
#include <libc/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "util.h"

void
debugline(const char *a, ...)
{
  va_list alist;
  FILE *fp;

  if(!(fp = fopen("C:\\debug", "a")))
    return;

  {
    // Get the time
    TTime now;
    now.HomeTime();
    TBuf<64> tbuf;
    _LIT(KFmt, "%F%H:%T:%S.%C");
    now.FormatL(tbuf, KFmt);

    // Convert it to ascii
    char mbuf[64];
    int i;
    for(i = 0; i < tbuf.Length(); i++)
      mbuf[i] = tbuf[i];
    mbuf[i] = 0;

    fprintf(fp, "%s ", mbuf);
  }

  va_start(alist, a);
  vfprintf(fp, a, alist);
  va_end(alist);
  fputc('\n', fp);
  fclose(fp);
}
#endif // _UTIL
