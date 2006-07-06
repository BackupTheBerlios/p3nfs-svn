#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>	/* for usleep() */
#include <string.h>	/* for bzero() */
#include <termios.h>
#if defined(linux) || defined(_IBMR2)
# include <sys/ioctl.h>	/* for ioctl() */
#endif
#ifdef __NeXT__
#include <sys/ioctl.h>
#include <libc.h>
#include <unistd.h>
#endif
#include <sys/errno.h>
#ifdef sun
# include <sys/ttold.h>	/* sun has TIOCEXCL there */
#endif
#if defined (__SVR4)
#include <stdlib.h>
#endif

#include "nfs_prot.h"
#include "mp.h"
#include "config.h"

extern int debug, dowakeup, errno;
#ifdef __NeXT__
static struct sgttyb sti;
#else
static struct termios sti;
#endif

#ifndef hpux
# define mflag int
# define getline(fd, z) if(ioctl(fd,TIOCMGET,(caddr_t)&z)<0){perror("TCGET");return 0;}
# define PSION_ALIVE(z) (z & (TIOCM_DSR | TIOCM_CD | TIOCM_CTS))
# define dtr(fd, set) { int d = TIOCM_DTR;\
		if(ioctl(fd, set ? TIOCMBIS : TIOCMBIC, (caddr_t)&d) < 0) {\
		perror("TIOCMBIC/S"); return 0; } }
#else /* hpux */
# include <sys/termiox.h>
# include <sys/modem.h>

# define getline(fd, z) if(ioctl(fd,MCGETA,&z)<0) {perror("MCGETA");return 0;}
# define PSION_ALIVE(z) (z & (MDSR | MDCD | MCTS))
# define dtr(fd, set) { if (set) z |= (mflag)MDTR; else z &= (mflag)~MDTR;\
      			if(ioctl(fd, MCSETA, &z) < 0) {\
			perror("MCSETA"); return 0; } }
#endif


/*
#if !defined(CRTSCTS) && defined(_IBMR2)
#define CRTSCTS 0x80000000
#endif
*/

#ifdef __sgi
#define CRTSCTS CNEW_RTSCTS
#endif

#ifndef O_NOCTTY
# define O_NOCTTY 0
#endif


/* returns 1 if OK. */
int 
fd_is_still_alive(fd, wake)
int fd, wake;
{
  int w;
  mflag z;
  extern int tcp;

  if(tcp)
    return 1;

  if (fd < 0) {
    if (debug > 2) printf("fd_is_alive: Checking on closed fd\n") ;
    return 0 ;
  }
  if(debug > 2) { printf("fd_is_alive: check lines "); fflush(stdout);}
  getline(fd, z)

  if(dowakeup)
    {
      if(debug > 2) printf("fd_is_alive: %s dtr\n", wake ? "set" : "clear");
      dtr(fd, wake);

      if(! PSION_ALIVE(z) && wake)
	{
	  if(debug > 1) printf("Trying to wake psion\n");
	  /* wake up psion by raising DTR */
	  dtr(fd, 1);

	  w = 10;
	  do
	    {
	      if(w < 10)
		usleep(100000);
	      getline(fd, z)
	    }
	  while(! PSION_ALIVE(z) && --w);
	  
	  if(debug > 1)
	    printf("Is %sawake (%d tries left)\n",PSION_ALIVE(z)?"":"not ",w);
	}
    }

  if(debug > 2) printf("fd_is_alive: %d\n", PSION_ALIVE(z));
  return PSION_ALIVE(z);
}

static int
get_speed(speed)
  int speed;
{
  static struct baud { int speed, baud; } btable[] =
    {
      { 50,	B50},
      { 75,	B75},
      { 300, 	B300},
      { 1200,	B1200},
      { 2400,	B2400},
      { 4800,	B4800},
      { 9600,	B9600},
#ifdef B19200
      { 19200,	B19200},
#endif
#ifdef EXTA
      { 19200,	EXTA},
#endif
#ifdef B38400
      { 38400,	B38400},
#endif
#ifdef EXTB
      { 38400,	EXTB},
#endif
#ifdef B57600
      { 57600,	B57600},
#endif
#ifdef B115200
      { 115200,	B115200},
#endif
      {0,0}
    }, *bptr;
  
  if(speed)
    {
      for (bptr = btable; bptr->speed; bptr++)
	if (bptr->speed == speed)
	  break;
      if (!bptr->baud)
	{
	  fprintf(stderr, "Cannot match selected speed %d\n", speed);
	  exit(1);
	}
      return bptr->baud;
    }
  return 0;
}

static int
init_background(dev)
  char *dev;
{
  int uid, euid, fd;

  if(debug) printf("using %s...\n", dev);
  euid = geteuid();
  uid = getuid();

#ifdef hpux
#define seteuid(a) setresuid(-1, a, -1)
#endif

  if (seteuid(uid))
    {
      perror("seteuid"); exit(1);
    }
  if((fd = open(dev, O_RDWR | O_NDELAY | O_NOCTTY , 0)) < 0)
    {
      perror(dev); exit(1);
    }
  if (seteuid(euid))
    {
      perror("seteuid back"); exit(1);
    }

  if(debug) printf("open done\n");
#ifdef TIOCEXCL
  ioctl(fd, TIOCEXCL, (char *) 0);	/* additional open() calls shall fail */
#else
  fprintf(stderr, "WARNING: opened %s non-exclusive!\n", dev);
#endif

  return fd;
}

void
close_012()
{
  char *f;
  
  close(0);                          /* Let's close 0,1,2 */
  f = "/dev/null";
  if(open(f, O_RDONLY, 0) != 0)
      {
       perror(f); exit(0);
     }
  close(1);
  f = "/tmp/p3nfsd.out";
  if(open(f, O_WRONLY|O_CREAT|O_APPEND, 0666) != 1)
      {
       perror(f); exit(0);
     }
  fprintf(stderr, "p3nfsd: output written to %s\n", f);
  close(2);
  dup(1);
}

#ifdef __NeXT__
int
init_serial()
{
  extern char *dev;
  extern int speed;
  int fd, baud, local;
  struct sgttyb ti;
  struct tchars        tchars;
  struct ltchars ltchars;

  baud = get_speed(speed);
  
  if(background)
  {
      fd = init_background(dev);
  }
  else
  {
      fd = dup(0);
  }
#define DO_IOCTL(x, arg) if(ioctl(fd, x, arg) < 0) { perror(#x); exit(1); }

  DO_IOCTL(TIOCGETP, &ti)
  DO_IOCTL(TIOCLGET, &local)
  DO_IOCTL(TIOCGETC, &tchars)
  DO_IOCTL(TIOCGLTC, &ltchars)

  local = LLITOUT | FLUSHO | PASS8OUT | PASS8;
  sti = ti;
  ti.sg_ispeed = ti.sg_ospeed = baud;
  ti.sg_flags = RAW;
  tchars.t_intrc = tchars.t_quitc = -1;
  ltchars.t_suspc = ltchars.t_dsuspc = ltchars.t_flushc
      = ltchars.t_lnextc = -1;

  DO_IOCTL(TIOCSETP, &ti)
  DO_IOCTL(TIOCLSET, &local)
  DO_IOCTL(TIOCSETC, &tchars)
  DO_IOCTL(TIOCSLTC, &ltchars)

  return fd;
}

void
reset_serial(fd)
int fd;
{
}

#else /*__NeXT__*/

int
init_serial()
{
  extern char *dev;
  extern int speed;
  int fd, baud, clocal;
  struct termios ti;
#ifdef hpux
  struct termiox tx;
#endif

  baud = get_speed(speed);
  
  if(background)
    {
      fd = init_background(dev);
      clocal = CLOCAL;
    }
  else
    {
      fd = dup(0);

      if(tcgetattr(fd, &ti) < 0)
        {
	  perror("tcgetattr serial");
	  exit(1);
	}
      sti = ti ;      /* Save the state for shutdown time */
      clocal = ti.c_cflag & CLOCAL ;  /* and for setup */

      close_012();
    }

  bzero((char *)&ti, sizeof(struct termios));
#if defined(hpux) || defined(_IBMR2)
  ti.c_cflag = CS8 | HUPCL | clocal | CREAD;
#warning soft_flow not implemented
#endif
#if defined(sun) || defined(linux) || defined(__sgi) || defined(__NetBSD__) || defined(__FreeBSD__)
  ti.c_cflag = CS8 | HUPCL | clocal | CREAD;
  ti.c_iflag = IGNBRK | IGNPAR;

  if(soft_flow)
    ti.c_iflag |= IXON|IXOFF;
  else if(!no_flow)
    ti.c_cflag |= CRTSCTS;

  ti.c_cc[VMIN] = 1;
  ti.c_cc[VTIME] = 0;
  ti.c_cc[VSTART] = SOFTFLOW_RSTART;
  ti.c_cc[VSTOP] = SOFTFLOW_RSTOP;
#endif
  cfsetispeed(&ti, baud);
  cfsetospeed(&ti, baud);


  if(tcsetattr(fd, TCSADRAIN, &ti) < 0)
    perror("tcsetattr TCSADRAIN");

#ifdef hpux
  bzero(&tx, sizeof(struct termiox));
  tx.x_hflag = RTSXOFF | CTSXON;
  if (ioctl(fd, TCSETXW, &tx) < 0)
    perror("TCSETXW");
#endif

#if defined(_IBMR2)
  ioctl(fd, TXDELCD, "dtr");
  ioctl(fd, TXDELCD, "xon");
  ioctl(fd, TXADDCD, "rts");  /* That's how AIX does CRTSCTS */
#endif

  return fd;
}

/*
 * We want to put the tty line back like we found it. It may be that
 * NetBSD is the only supported OS that needs this done to it.
 */
void
reset_serial(fd)
int fd;
{
  if(!background && fd >= 0)
    if(tcsetattr(fd, TCSANOW, &sti) < 0)
      perror("tcsetattr TCSANOW");
}
#endif /*!__NeXT__*/


#if 0
#ifdef sun
/*
 * Before exiting we should close the serial line file descriptor
 * as we are probably on a "wicked sun", we have to reset the CRTSCTS
 * flag first, otherwise we will hang in close(2).
 * (suns that are not "wicked" are "broken" , i.e.: they do not have
 * patch 100513-04. There are also suns that are "wicked" and "broken".
 * These run Solaris >= 2.0 !! * YESSSSSS!!! gec
 */

void
ser_exit(status,fd)
int status,fd;
{
  struct termios ti;
  if(ioctl(fd, TCGETS, (caddr_t)&ti) < 0) {
    perror("TCGETSW");
  }
  ti.c_cflag &= ~CRTSCTS;
  if(ioctl(fd, TCSETS, (caddr_t)&ti) < 0) {
    perror("TCSETSW");
  }
  (void) close(fd);
}
#endif /* sun */
#endif
