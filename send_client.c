#include <stdio.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <netdb.h>
#include <stdlib.h>

#define PORT_NO 8000
#define MAX_BUF_SIZE 1024

ssize_t writen(int fd,const void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;

  //現在の文字列の位置
  ptr = vptr;

  //残りの文字列の長さの初期化
  nleft = n;
  while (nleft > 0) {
    if ((nwritten = write(fd, ptr, nleft)) <= 0) {
      if (nwritten < 0 && errno == EINTR) {
	nwritten = 0;  // try again
      } else {
	return -1;
      }
    }
    nleft -= nwritten;
    ptr += nwritten;
  }
  return n;
}

int main (int argc, char *argv[])
{
  char buf[MAX_BUF_SIZE];
  int buf2[256];
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  buf[MAX_BUF_SIZE-1] = '\0';

  if (fd < 0) {
    perror("socket");
    return -1;
  }

  struct hostent *hp;
  if ((hp = gethostbyname(argv[1])) == NULL) {
    fprintf(stderr, "gethost error %s\n", argv[1]);
    close(fd);
    return -1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT_NO);
  memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);
  while (1) {
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	  continue;
    } else {
      break;
    }
  }

  char input = getchar();

  for (int a = 0; a < MAX_BUF_SIZE; a++) {
	buf[a] = input;
  }

  buf[MAX_BUF_SIZE - 1] = '\0';

  for (int i = 0; i < 1000; i++) {
	writen(fd, buf, sizeof(buf));
  }


  if (close(fd) == -1) {
	printf("%d\n", errno);
  }

  return 0;
}
