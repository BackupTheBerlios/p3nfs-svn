#include <OSdefs.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include "nfs_prot.h"
#include "mp.h"
#include "version.h"
#include "config.h"
#include <string.h>		/* strcmp */
#if defined (__SVR4) || defined(__sgi)
#include <stdlib.h>		/* getenv */
#endif
#include <unistd.h>		/* getuid */

#include <dirent.h>
#if defined(__NeXT__)
#include <sys/dir.h>
#include <unistd.h>
#define DIRENT struct direct
#else
#define DIRENT struct dirent
#endif


extern void nfs_program_2();


char *dev  = DDEV, *user, *dir  = DDIR;
int speed = 115200;
int timeout = 5000;	// 5 seconds

int psionfd, gmtoffset, debug, exiting, psion_alive, dowakeup = 0, old_nfsc = 0,
    query_cache = 0, background = 1, series5 = 0, soft_flow = 0, no_flow = 0,
    tcp = 0, nfsblock = PBUFSIZE;
fattr root_fattr = {
  NFDIR, 0040500, 1, 0, 0,
  BLOCKSIZE, BLOCKSIZE, FID, 1, FID, 1,
  {0, 0},
  {0, 0},
  {0, 0}
};

char *disconnprog, *connprog, *shell = 0;

#if defined(hpux) || defined(__SVR4) || defined(__sgi)
void
usleep(usec)
int usec;
{
  struct timeval t;

  t.tv_sec = (long) (usec / 1000000);
  t.tv_usec= (long) (usec % 1000000);
  select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &t);
}
#endif /* hpux */

int
main(ac, av)
  int ac;
  char *av[];
{
  struct passwd *pw;
  struct timeval tv;
  struct timezone tz;
  p_inode *rp;
  mynfs_fh root_fh;
  DIR *dirp;
  DIRENT *diep;
  int i, arch_set = 0;


  if(!(user = (char *)getenv("USER")))
    user = (char *)getenv("logname");
    
  while(ac > 1)
    {
      if(!strcmp(av[1], "-v"))            { debug++; }
      else if(!strcmp(av[1], "-nobackground")) { background = 0; }
      else if(!strcmp(av[1], "-dir"))     { dir  = av[2];        ac--;av++; }
      else if(!strcmp(av[1], "-user"))    { user = av[2];        ac--;av++; }
      else if(!strcmp(av[1], "-tty"))     { dev = av[2]; tcp = 0; ac--;av++; }
      else if(!strcmp(av[1], "-speed"))   { speed = atoi(av[2]); ac--; av++; }
      else if(!strcmp(av[1], "-tcp"))     { tcp = 1; dev = 0; speed = 0; }
      else if(!strcmp(av[1], "-conn"))    { connprog = av[2];    ac--; av++; }
      else if(!strcmp(av[1], "-disconn")) { disconnprog = av[2]; ac--; av++; }
#ifdef PTY
      else if(!strcmp(av[1], "-wakeup"))  { dowakeup = 1; }
      else if(!strcmp(av[1], "-shell"))   { shell = av[2];       ac--;av++; }
#endif
      else if(!strcmp(av[1], "-oldnfsc")) { old_nfsc = 1; }
      else if(!strcmp(av[1], "-noflow"))  { no_flow = 1; }
      else if(!strcmp(av[1], "-softflow")){ soft_flow = 1; }
      else if(!strcmp(av[1], "-timeout")) { timeout = atoi(av[2]); ac--; av++; }
      else if(!strcmp(av[1], "-epoc32_filesystem")){ series5 = 1; }
      else if(!strcmp(av[1], "-nfsblock")) { nfsblock = atoi(av[2]); ac--;av++;}

      else if(!strcmp(av[1], "-series3")) { speed = 9600; arch_set = 1;  }
      else if(!strcmp(av[1], "-series3a")){ speed = 19200; arch_set = 1; }
      else if(!strcmp(av[1], "-series5"))
        {
	  no_flow = 1;
	  old_nfsc = 1;
	  series5 = 1;
	  arch_set = 1;
	}
      else if(!strcmp(av[1], "-series60") ||
              !strcmp(av[1], "-series80") ||
              !strcmp(av[1], "-UIQ"))
        {
	  if(strcmp(av[1], "-series80"))
	    dev = "/dev/ircomm0";
	  no_flow = 1;
	  old_nfsc = 1;
	  series5 = 1;
	  arch_set = 1;
	  nfsblock = 4096;
	}
      else
	{
	  printf("p3nfsd version %s\n", VERSION); 
	  printf("Usage: p3nfsd {-series3|-series3a|-series5|-series60|-series80|-UIQ}\n");
	  printf("   [-v] [-v] [-v]\n");
	  printf("   [-nobackground]\n");
	  printf("   [-dir <directory>] [-user <uid>]\n");
	  printf("   [-tty <dev>] [-speed <baud>] [-noflow]\n");
	  printf("   [-tcp]\n");
	  printf("   [-timeout <msec>]\n");
	  printf("   [-oldnfsc] [-epoc32_filesystem]\n");
	  printf("   [-conn <prog>] [-disconn <prog>] (if HW flowcontrol available)\n");
	  printf("   [-nfsblock <NFS bloksize>]\n");
#ifdef PTY
	  printf("Series 3a client only with nfsc:\n");
	  printf("   [-wakeup] [-shell prog]\n");
#endif
	  printf("Current settings:\n");
	  if(dir)             printf("  -dir %s\n", dir);
	  if(user)            printf("  -user %s\n", user);
	  if(tcp)	      printf("  -tcp\n");
	  if(dev)             printf("  -tty %s\n", dev);
	  if(speed)           printf("  -speed %d\n", speed);
	  if(dowakeup)        printf("  -wakeup\n");
	  if(old_nfsc)        printf("  -oldnfsc\n");
	  if(series5)         printf("  -epoc32_filesystem\n");
	  if(soft_flow)       printf("  -softflow\n");
	  if(no_flow)         printf("  -noflow\n");
	  if(timeout)         printf("  -timeout %d\n", timeout);
	  printf("  -nfsblock %d\n", nfsblock);
	  return 1;
	}
      ac--;av++;
    }
  

  if(!arch_set)
    {
      printf("Please specify one of the following options (architecture):\n");
      printf("   -series3, -series3a, -series5, -series60, -series80, -UIQ\n");
      printf("See \"p3nfsd -h\" or \"p3nfsd <architecture> -h\" for more\n");
      exit(1);
    }

  if (user && *user)
    {
      if(!(pw = getpwnam(user)))
	{
	  fprintf(stderr, "User %s not found.\n", user);
	  return 1;
	}
    }
  else if(!(pw = getpwuid(getuid())))
    {
      fprintf(stderr, "You don't exist, go away! (getpwuid error)\n");
      return 1;
    }
  if(getuid() && pw->pw_uid != getuid())
    {
      fprintf(stderr, "%s? You must be kidding... (Try again as root)\n", user);
      return 1;
    }
  root_fattr.uid = pw->pw_uid;
  root_fattr.gid = pw->pw_gid;
  endpwent();


  gettimeofday(&tv, &tz);
#ifndef __SVR4
  gmtoffset = -tz.tz_minuteswest * 60;
#else
  tzset();
  gmtoffset = -timezone;
#endif

  if(!background)
    dev = "stdin";
  if(speed == 0 && background)
    speed = DSPEED;
  if(tcp)
    printf("p3nfsd: version %s, using tcp port 13579, mounting on %s\n",
    	VERSION, dir);
  else
    printf("p3nfsd: version %s, using %s (%d), mounting on %s\n",
  	VERSION, dev, speed, dir);


  /* Check if mountdir is empty (or else you can overmount e.g /etc)
     It is done here, because exit hangs, if hardware flowcontrol is
     not present. Bugreport Nov 28 1996 by Olaf Flebbe */
  if(!(dirp = opendir(dir)))
    {
      perror(dir);
      return 1;
    }
  i = 0;
  while((diep = readdir(dirp)) != 0)
    if(strcmp(diep->d_name, ".") && strcmp(diep->d_name, ".."))
      i++;
  closedir(dirp);
  if(i)
    {
      fprintf(stderr, "Sorry, directory %s is not empty, exiting.\n", dir);
      return 1;
    }


  if (!old_nfsc && shell)
    init_pty();

  if(tcp)
    psionfd = init_tcp();
  else
    psionfd = init_serial();

  rp = get_nam("");
  inode2fh(rp->inode, root_fh.data);
  root_fattr.fileid = rp->inode;
  root_fattr.atime.seconds = root_fattr.mtime.seconds = 
  root_fattr.ctime.seconds = tv.tv_sec;

  mount_and_run(dir, dev, nfs_program_2, &root_fh);
  return 0;
}
