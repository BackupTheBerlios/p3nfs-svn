#ifdef __FreeBSD__
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <stdio.h>
#endif

#if defined(__NeXT__)
# include <libc.h>
# include <string.h>
# include <objc/hashtable.h>
# define strdup NXCopyStringBuffer
#endif

#if defined(linux)
#define mynfs_fh nfs2_fh 
#else
#define mynfs_fh nfs_fh
#endif
