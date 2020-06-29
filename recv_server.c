#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <unistd.h>


#define SERVER_PORT 9872
#define MAX_EVENTS 3000
#define BACKLOG 10
#define MEM_SIZE 1024 * 1024
#define BUF_SIZE 1024
#define TALE_MAX 1000
#define TALE 512

#define rdtsc_64(lower, upper) asm __volatile ("rdtsc" : "=a"(lower), "=d" (upper));

static int listener;
static int epfd;
//int packet_log[MAX_COUNT][256];
char *tear;

static void die(const char* msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}

static int setup_socket()
{
  int sock;
  struct sockaddr_in sin;

  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    die("socket");
  }

  int on =1;
  int ret;
  ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  memset(&sin, 0, sizeof sin);
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(SERVER_PORT);

  if (bind(sock, (struct sockaddr *) &sin, sizeof sin) < 0) {
    close(sock);
    die("bind");
  }

  if (listen(sock, BACKLOG) < 0) {
    close(sock);
    die("listen");
  }

  return sock;
}

char *setup_shmem()
{
  const char file_name[] = "test.dat";
  const int id = 52;
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
  tear = shared_memory;

  return shared_memory;
}

/* Read "n" bytes from a descriptor. */

ssize_t readn(int fd, void *buf, size_t count)
{
  int *ptr = buf;
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

void teardown()
{
  /*
  for (int k = 0; k < MAX_COUNT; k++) {
	for(int j = 0; j < 256; j++){
	  if (packet_log[k][j] != k) {
		printf("round: %d, ofset: %d, value: %d\n", k, j, packet_log[k][j]);
	  }
	}
  }
  printf("%d\n", errno);
  */
  /*
  printf("aa");
  shmdt(tear);
  */
  exit(0);
}


int main()
{
  struct epoll_event ev;
  struct epoll_event events[MAX_EVENTS];
  char buffer[BUF_SIZE];
  int count = 0;
  int head= 1, tale = 1;
  char *shared_memory;
  unsigned int tsc_l, tsc_u; //uint32_t
  unsigned long int tsc; //uint64_t

  signal(SIGINT, teardown);
  if ((epfd = epoll_create(MAX_EVENTS)) < 0) {
    die("epoll_create");
  }

  listener = setup_socket();
  printf("listener: %d\n", listener);
  shared_memory = setup_shmem("test.dat", 52);
  if(shared_memory == NULL) {
	return -1;
  }
  memcpy(shared_memory, &head, sizeof(head));
  memcpy((shared_memory + TALE), &tale, sizeof(tale));
  memset(&ev, 0, sizeof ev);
  ev.events = EPOLLIN;
  ev.data.fd = listener;
  epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev);

  for (;;) {
    int nfd = epoll_wait(epfd, events, MAX_EVENTS, -1);

    for (int i = 0; i < nfd; i++) {
      if (events[i].data.fd == listener) {
	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof client_addr;

	int client = accept(listener, (struct sockaddr *) &client_addr, &client_addr_len);
	if (client < 0) {
	  perror("accept");
	  continue;
	}
	printf("client: %d\n",client);

	memset(&ev, 0, sizeof ev);
	ev.events = EPOLLIN;
	ev.data.fd = client;
	epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev);
      } else {
		int client = events[i].data.fd;
		count++;
		while((head - 1) == tale || ((head == 1) && (tale == TALE_MAX))) {
		  memcpy(&head, shared_memory, sizeof(head));
		  memcpy(&tale, shared_memory + TALE, sizeof(tale));
		}
		unsigned long int log_tsc;
		int n = readn(client, buffer, sizeof buffer);
		rdtsc_64(tsc_l, tsc_u);
		log_tsc = (unsigned long int)tsc_u<<32 | tsc_l;
		memcpy(buffer + sizeof(log_tsc), &log_tsc, sizeof(log_tsc));
		/*		for (int h = 0; h < 256; h++) {
		  packet_log[count][h] = buffer3[count][h];
		  }*/
		if (n <= 0) {
		  if (n < 0) perror("read");
		  epoll_ctl(epfd, EPOLL_CTL_DEL, client, &ev);
		  close(client);
		  /*		  for(int k=0; k<count; k++){
			for(int j=0; j<256; j++){
			  printf("%d,", buffer3[k][j]);
			}
			printf("\n");
		  }
		  */
		} else {
		  memcpy((shared_memory + tale * BUF_SIZE), buffer, BUF_SIZE);
		  if(tale == TALE_MAX) {
			tale = 1;
		  } else {
			tale++;
		  }
		  memcpy((shared_memory + TALE), &tale, sizeof(tale));
		}
      }
    }
  }
  return 0;
}
