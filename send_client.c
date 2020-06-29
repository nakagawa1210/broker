#include <stdio.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <netdb.h>
#include <stdlib.h>

#define rdtsc_64(lower, upper) asm __volatile ("rdtsc" : "=a"(lower), "=d" (upper));

#define PORT_NO 9872
#define MAX_BUF_SIZE 1024
#define MAX_COUNT 50000
#define MEM_SIZE 1024

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

char *setup_shmem()
{
  const char file_name[] = "test.dat";
  const int id = 53;
  char *shared_memory;
  int seg_id;
  key_t key;
  FILE *fp;

  if((fp = fopen(file_name, "w")) == NULL){
	fprintf(stderr, "error file open\n");
	return NULL;
  }
  fclose(fp);

  if((key = ftok(file_name, id)) == -1){
	fprintf(stderr, "error get key\n");
	return NULL;
  }

  if((seg_id = shmget(key, MEM_SIZE, IPC_CREAT | S_IRUSR | S_IWUSR)) == -1){
	fprintf(stderr, "error get shm\n");
	return NULL;
  }

  shared_memory = (char*)shmat(seg_id, 0, 0);

  return shared_memory;
}


void send_msg (char *host)
{
  char buf[MAX_BUF_SIZE];
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  buf[MAX_BUF_SIZE-1] = '\0';

  if (fd < 0) {
    perror("socket");
    return;
  }

  struct hostent *hp;
  if ((hp = gethostbyname(host)) == NULL) {
    fprintf(stderr, "gethost error %s\n", host);
    close(fd);
    return;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT_NO);
  memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);

  //  char input = getchar();
  char input = 'a';

  while (1) {
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	  continue;
	} else {
	  break;
	}
  }

  for (int a = 0; a < MAX_BUF_SIZE; a++) {
	buf[a] = input;
  }

  unsigned int tsc_l, tsc_u; //uint32_t
  unsigned long int tsc; //uint64_t
  unsigned long int log_tsc;

  for (int i = 0; i < MAX_COUNT; i++) {
	rdtsc_64(tsc_l, tsc_u);
	log_tsc = (unsigned long int)tsc_u<<32 | tsc_l;
	memcpy(buf, &log_tsc, sizeof(log_tsc));
	writen(fd, buf, sizeof(buf));
  }

  if (close(fd) == -1) {
	printf("%d\n", errno);
  }

}



int main (int argc, char *argv[])
{
  int i, flag = 1;
  char *shared_memory;
  shared_memory = setup_shmem();
  char start_input = getchar();
  for (i = 0; i < 10; i++) {
	while (flag == 0) {
	  memcpy(&flag, shared_memory, sizeof(flag));
	  sleep(1);
	}
	flag = 0;
	memcpy(shared_memory, &flag, sizeof(flag));
	send_msg(argv[1]);
	sleep(5);
  }
  return 0;
}
