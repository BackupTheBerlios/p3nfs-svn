#include <OSdefs.h>
#include "config.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(__SVR4) || defined(__GLIBC__)
#include <string.h> /* Solaris 2.4 does not have a strings.h */
#endif

#ifndef PTY
void
shell_feed(i)
  int i;
{
  printf("shellfeed: psionbyte %#x (%c)\n", i, i);
}

void
init_pty()
{
}
#else /* PTY */

#ifdef linux
#include <sys/ioctl.h>	/* Linux ioctl */
#endif
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>

#include "nfs_prot.h"
#include "mp.h"

int masterfd = -1;

/* open a pty and start a shell in it */
void
init_pty()
{
  int fd;
  char name[64], fnd[3], *s, *t;
  extern char *shell;
#ifdef __NeXT__
  struct sgttyb ti;
  struct tchars        tchars;
  struct ltchars ltchars;
#else
  struct termios tio;
#endif
  char **newenv;
  int n, nn;
  extern char **environ;
  char term[16];

  /* first of all check if the shell is executable */
  if(access(shell, X_OK))
    {
      perror(shell);
      return;
    }

#ifdef __NeXT__
  if (ioctl(0, TIOCGETP, &ti) < 0)
      {
          perror("TIOCGETP pty"); return;
      }
  if (ioctl(0,TIOCGETC,&tchars) < 0)
      {
          perror("TIOCGETC pty"); return;
      }
  if (ioctl(0,TIOCGLTC,&ltchars) < 0)
      {
          perror("TIOCGLTC pty"); return;
      }
#else
  if (tcgetattr(0, &tio))
    {
      perror("tcgetattr");
      return;
    }
#endif

#ifdef __sgi
  s = _getpty(&fd, O_RDWR | O_NOCTTY, 0666, 1);
  if (s == 0)
    return;
  strcpy(name, s);
#else
  fd = -1;
  for(s = "pqrs"; fd < 0 && *s; s++)
    for(t = "01234567890abcdef"; *t; t++)
      {
	sprintf(fnd, "%c%c", *s, *t);
	sprintf(name, "/dev/pty%s", fnd);
	if((fd = open(name, O_RDWR)) >= 0)
	  break;
      }
  sprintf(name, "/dev/tty%s", fnd);
#endif

  if(fd < 0)
    {
      perror("open pty");
      return;
    }

  switch(fork())
    {
    case -1:
      perror("fork");
      close(fd);
      return;
    case 0:
      break;
    default:
      masterfd = fd;
      return;
    }
  close(fd);

#ifdef __NeXT__
  {
      int ttyfd;
      ttyfd = open("/dev/tty", O_RDWR);
      ioctl(ttyfd, TIOCNOTTY, 0);
      close(ttyfd);
  }
#else
   setsid();    /* create new session, disconnect from ctty */
#endif

  setsid();	/* create new session, disconnect from ctty */

  close(0);
  if(open(name, O_RDWR) != 0)
    {
      perror(name);
      exit(1);
    }
  close(1);
  close(2);
  dup(0);
  dup(0);

  /*
   * The pty we just opened REALLY ought to be our controlling terminal.
   * If we're running on a system that let's us set it explicitly, do that,
   * as there's a reasonable chance it didn't do it for us on open.
   */
#ifdef        TIOCSCTTY
  ioctl(0, TIOCSCTTY, (char *) 0);
#endif

#ifdef __NeXT__
  if (ioctl(0,TIOCSETP,&ti) < 0)
      {
          perror("TIOCSETP pty"); return;
      }
  if (ioctl(0,TIOCSETC,&tchars) < 0)
      {
          perror("TIOCSETP pty"); return;
      }
  if (ioctl(0,TIOCSLTC,&ltchars) < 0)
      {
          perror("TIOCSETP pty"); return;
      }
#else
  tcsetattr(0, TCSANOW, &tio);
#endif

  setgid(root_fattr.gid);
  setuid(root_fattr.uid);

  /* count the environment */
  for (n = 0; environ[n]; n++)
    ;
  if ((newenv = (char **)malloc((n + 2) * sizeof(char *))) == 0)
    {
      perror("env malloc");
      exit(1);
    }
  for (n = nn = 0; environ[n]; n++)
    {
      if (strncmp(environ[n], "TERM=", 5) == 0)
	continue;
      if (strncmp(environ[n], "TERMCAP=", 8) == 0)
	continue;
      if (strncmp(environ[n], "LINES=", 6) == 0)
	continue;
      if (strncmp(environ[n], "COLUMNS=", 8) == 0)
	continue;
      newenv[nn++] = environ[n];
    }

  sprintf(term, "TERM=vt220");
  newenv[nn++] = term;
  newenv[nn] = 0;

  execle(shell, shell, 0, newenv);
  perror(shell);
  exit(1);
}

void
shell_feed(i)
  int i;
{
  static int resize = 0;
  unsigned char c = i;
  static struct winsize ws;

  if(debug > 1) printf("shellfeed: psionbyte %#x (%c)\n", i, i);
  if(masterfd < 0)
    return;

  switch(resize)
    {
      case 0:
        if(i == 0)		/* resize prefix */
	  resize++;
	else
	  write(masterfd, &c, 1);
	break;
      case 1: 			/* receiving x */
        ws.ws_xpixel = 0; ws.ws_ypixel = 0;
        ws.ws_row = i;
	resize++;
	return;
      case 2:			/* receiving y */
        ws.ws_col = i;
	resize = 0;
        if(debug) printf("Resize to %d x %d\n", ws.ws_row, ws.ws_col);
	ioctl(masterfd, TIOCSWINSZ, (char *)&ws);
	return;
    }
}

#endif /* PTY */
