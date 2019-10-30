#include <stdio.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>

#define PORT_NO 8001
#define MAX_BUF_SIZE 1024
#define MAX_COUNT 1000

/* Read "n" bytes from a descriptor. */
ssize_t readn(int fd, void *buf, size_t count)
{
  char *ptr = buf;
  size_t nleft = count;
  ssize_t nread;

  while (nleft > 0) {
    if ((nread = read(fd, ptr, nleft)) < 0) {

      if (errno == EINTR)
        continue;
      else
        return -1;
    }

    if (nread == 0) {
      return count - nleft;
    }
    nleft -= nread;
    ptr += nread;
  }
  return count;
}

int main (int argc, char *argv[])
{
  char buf[MAX_COUNT][MAX_BUF_SIZE];
  int count = 0;
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  uint64_t start = 0;
  uint64_t end = 0;

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
	  sleep(1);
      continue;
    } else {
      break;
    }
  }
  printf("connect success\n");

  while(1) {
	if(readn(fd, buf[count], sizeof buf[count]) < 0) {
	  close(fd);
	  break;
	}
  }

  for (int k = 0; k < MAX_COUNT; k++) {
	for(int j = 0; j < MAX_BUF_SIZE; j++){
	  if(buf[k][j] != 'a') {
		printf("round: %d, ofset: %d, value: %c\n", k, j, buf[k][j]);
	  }
	}
  }

  return 0;
}
