/*
 * defs.h.in -- customize p3nfsd here, if you really need to.
 * configure should do most of the work for you.
 */

#define PTY	/* Enable vt100 support parallel with nfsd */

#define MTAB_PATH	"/etc/mtab"
#ifdef linux
# define MTAB_TMP	"/etc/mtab~"
#else
# define MTAB_TMP	"/etc/mtab.p3nfsd"
#endif

#define DDEV		"/dev/ttyS0"
/*
 * hpux10		/dev/tty0p0
 * hpux			/dev/tty00
 * linux		/dev/ttyS0
 * sun && __svr4__ 	/dev/term/a
 * NeXT 		/dev/ttyfa
 * sun  		/dev/ttya
 * _IBMR2		/dev/tty0
 * __sgi		/dev/ttyf1
 * FreeBSD 3.1		/dev/cuaa1
 */

#ifdef _IBMR2
# define DONT_UPDATE_MTAB /* The mount table is obtained from the kernel (!?) */
#endif

#define DUSER "root"
#ifndef DDEV
ERROR: p3nfsd is not yet ported to this system
#endif

#ifndef DDIR
# define DDIR "/mnt/psion"
#endif

#ifndef PSIONHOSTNAME
# define PSIONHOSTNAME "localhost"
#endif


#define DSPEED 19200

/* See CHANGES for comment */
//#ifdef linux
//#define NO_WRITE_SELECT
//#endif
