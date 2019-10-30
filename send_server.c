#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>

#define SERVER_PORT 8001
#define MAX_EVENTS 3000
#define BACKLOG 10
#define MAX_DATA 1000
#define MAX_COUNT 1000
#define MAX_BUF_SIZE 1024
#define MAX_FD_SIZE 1024
#define HEAD_MAX 1024
#define TALE 512

static int listener;
static int epfd;

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

/* Write "n" bytes from a descriptor. */
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

int main()
{
  struct epoll_event ev;
  struct epoll_event events[MAX_EVENTS];
  char buffer[MAX_BUF_SIZE];
  int count[MAX_FD_SIZE];
  int head = 1, tale = 1;
  char *shared_memory;

  if ((epfd = epoll_create(MAX_EVENTS)) < 0) {
    die("epoll_create");
  }

  shared_memory = setup_shmem("test.dat", 52);
  if(shared_memory == NULL) {
	return -1;
  }
  listener = setup_socket();
  printf("listener: %d\n", listener);
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

		printf("accept: %d\n", client);

		count[client] = 1;

		memset(&ev, 0, sizeof ev);
		ev.events = EPOLLOUT;
		ev.data.fd = client;
		epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev);
      } else {
		while(head == tale) {
		  head = atoi(shared_memory);
		  tale = atoi((shared_memory + TALE));
		  printf("%d", head);
		  sleep(1);
		}
		int client = events[i].data.fd;
		sprintf(buffer, "%s", (shared_memory + head * MAX_BUF_SIZE));

		if(head == HEAD_MAX) {
		  head = 1;
		} else {
		  head++;
		}
		printf("head:%d tale:%d sh_head:%d\n", head, tale, atoi(shared_memory));

		sprintf(shared_memory, "%d", head);

		int n = writen(client, buffer, sizeof buffer);
		if (n <= 0) {
		  if (n < 0) perror("write");
		  epoll_ctl(epfd, EPOLL_CTL_DEL, client, &ev);
		  close(client);

		} else {
		  count[client]++;
		  if (count[client] > MAX_COUNT) {
			close(client);
		  }
		}
      }
    }
  }
  return 0;
}
