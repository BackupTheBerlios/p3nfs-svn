#ifdef linux
# include <unistd.h>		/* read() write() gethostname() */
# include <arpa/inet.h>		/* inet_addr() */
# include <string.h>		/* bcopy() */
#endif
#ifdef __SVR4
#define bcopy(a,b,c) memcpy(b,a,c)
#endif
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>


/*
 * Opens a TCP server port (13579), waits indefinitely for a client
 * connect, and returns the fd if he got it. Exits if an error occurs
 */
int
init_tcp()
{
  int uid, euid, port = 13579;
  struct protoent *pe;
  struct sockaddr_in sa;
  int ia, listen_sock, sock;

  euid = geteuid();
  uid = getuid();

#ifdef hpux
#define seteuid(a) setresuid(-1, a, -1)
#endif
  if(seteuid(uid))
    {
      perror("seteuid");
      exit(1);
    }

  sa.sin_addr.s_addr=INADDR_ANY;
  sa.sin_family = PF_INET;
  sa.sin_port = htons(port);
  if((pe = getprotobyname("tcp")) == 0)
    {
      perror("getprotobyname");
      exit(1);
    }
  if((listen_sock = socket(PF_INET, SOCK_STREAM, pe->p_proto)) < 0)
    {
      perror("socket");
      exit(1);
    }
  ia = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&ia, sizeof(ia));
  if(bind(listen_sock, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
      perror("bind");
      exit(1);
    }
  ia = sizeof(struct sockaddr_in);
  if(getsockname(listen_sock, (struct sockaddr *)&sa, &ia) < 0)
    {
      perror("getsockname");
      return -1;
    }
  if(listen(listen_sock, 1))
    {
      perror("listen"); 
      return -1;
    }

  ia = sizeof(struct sockaddr_in);
  if((sock = accept(listen_sock, (struct sockaddr *)&sa, &ia)) < 0)
    {
      perror("accept");
      exit(1);
    }
  close(listen_sock);
  if(seteuid(euid))
    {
      perror("seteuid back");
      exit(1);
    }
  return sock;
}
