#include <stdio.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#define rdtsc_64(lower, upper) asm __volatile ("rdtsc" : "=a"(lower), "=d" (upper));

#define CLOCK_HZ 2600000000.0
#define PORT_NO 9873
#define MAX_BUF_SIZE 1024
#define MAX_COUNT 50000

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

char *setup_shmem(char file_name[], int id)
{
  char buf[MAX_BUF_SIZE];
  char * shared_memory;
  int seg_id, flag;
  key_t key;

  key = ftok(file_name, id);

  if((seg_id = shmget(key, 0, 0)) == -1){
	fprintf(stderr,"error shmget");
	return NULL;
  }

  shared_memory = (char*)shmat(seg_id, 0, 0);

  return shared_memory;
}

void recv_msg (char *host)
{
  char buf[MAX_BUF_SIZE];
  uint64_t recv_time[MAX_COUNT];
  int count = 0;
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  uint64_t end = 0;

  if (fd < 0) {
    perror("socket");
    return;
  }

  struct hostent *hp;
  if  ((hp = gethostbyname(host)) == NULL) {
    fprintf(stderr, "gethost error %s\n", host);
    close(fd);
    return;
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

  unsigned int tsc_l, tsc_u; //uint32_t
  unsigned long int tsc; //uint64_t
  unsigned long int log_tsc[4][MAX_COUNT];
  while(1) {
	if(readn(fd, buf, sizeof buf) <= 0) {
	  close(fd);
	  break;
	}
	rdtsc_64(tsc_l, tsc_u);
	log_tsc[3][count] = (unsigned long int)tsc_u<<32 | tsc_l;
	memcpy(&log_tsc[0][count], buf, sizeof(unsigned long int));
	memcpy(&log_tsc[1][count], buf + sizeof(unsigned long int), sizeof(unsigned long int));
	memcpy(&log_tsc[2][count], buf + 2 * sizeof(unsigned long int), sizeof(unsigned long int));
	count++;
  }

  unsigned long int start;

  start = log_tsc[0][0];

  for (int i = 0; i < MAX_COUNT; i++) {
	printf("%lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf\n",
		   (log_tsc[0][i] - start) / CLOCK_HZ,
		   (log_tsc[1][i] - start) / CLOCK_HZ,
		   (log_tsc[2][i] - start) / CLOCK_HZ,
		   (log_tsc[3][i] - start) / CLOCK_HZ,
		   (log_tsc[3][i] - log_tsc[0][i])/CLOCK_HZ,
		   (log_tsc[1][i] - log_tsc[0][i])/CLOCK_HZ,
		   (log_tsc[2][i] - log_tsc[1][i])/CLOCK_HZ,
		   (log_tsc[3][i] - log_tsc[2][i])/CLOCK_HZ);
  }
  return;
}

int main(int argc, char *argv[])
{
  char *shared_memory;
  int i, flag = 1;
  shared_memory = setup_shmem("test.dat", 53);
  printf("prd_time, recv_time, send_time, cons_time, prd2cons_time, prd2recv_time, recv2send_time, send2cons_time\n");
  for (i = 0; i < 10; i++) {
	recv_msg(argv[1]);
	memcpy(shared_memory, &flag, sizeof(flag));
  }
  return 0;
}
