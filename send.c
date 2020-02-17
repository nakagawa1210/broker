/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MIT
 *
 * Portions created by Alan Antonuk are Copyright (c) 2012-2013
 * Alan Antonuk. All Rights Reserved.
 *
 * Portions created by VMware are Copyright (c) 2007-2012 VMware, Inc.
 * All Rights Reserved.
 *
 * Portions created by Tony Garnock-Jones are Copyright (c) 2009-2010
 * VMware, Inc. and Tony Garnock-Jones. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
*
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ***** END LICENSE BLOCK *****
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>

#include "utils.h"

#define SUMMARY_EVERY_US 10000000
#define MEM_SIZE 1024
#define MESSAGE_SIZE 2048

#define rdtsc_64(lower, upper) asm __volatile ("rdtsc" : "=a"(lower), "=d" (upper));

static void send_batch(amqp_connection_state_t conn, char const *queue_name,
                        int message_count) {
  // uint64_t start_time = now_microseconds();
  int i;
  //  int sent = 0;
  int previous_sent = 0;
  //  uint64_t previous_report_time = start_time;
  // uint64_t next_summary_time = start_time + SUMMARY_EVERY_US;

  char message[MESSAGE_SIZE];
  amqp_bytes_t message_bytes;

  for (i = 0; i < sizeof(message); i++) {
    message[i] = 'a';
  }


  unsigned long int log_tsc;
  unsigned int tsc_l, tsc_u; //uint32_t
  unsigned long int tsc; //uint64_t
  for (i = 0; i < message_count; i++) {
  rdtsc_64(tsc_l, tsc_u);
  log_tsc = (unsigned long int)tsc_u<<32 | tsc_l;
  memcpy(message, &log_tsc, sizeof(log_tsc));
  message_bytes.len = sizeof(message);
  message_bytes.bytes = message;
    die_on_error(amqp_basic_publish(conn, 1, amqp_empty_bytes,
                                    amqp_cstring_bytes("hello"), 0, 0, NULL,
                                    message_bytes),
                 "Publishing");
	//   sent++;
	/*
    if (now > next_summary_time) {
      int countOverInterval = sent - previous_sent;
      double intervalRate =
          countOverInterval / ((now - previous_report_time) / 1000000.0);
      printf("%d ms: Sent %d - %d since last report (%d Hz)\n",
             (int)(now - start_time) / 1000, sent, countOverInterval,
             (int)intervalRate);

      previous_sent = sent;
      previous_report_time = now;
      next_summary_time += SUMMARY_EVERY_US;
    }
	*/
	/*
    while (((i * 1000000.0) / (now - start_time)) > rate_limit) {
      microsleep(2000);
      now = now_microseconds();
    }
	*/
  }

  {
	//   uint64_t stop_time = now_microseconds();
	//   int total_delta = (int)(stop_time - start_time);


      printf("PRODUCER - Message count: %d\n", message_count);
	//  printf("Total time, milliseconds: %d\n", total_delta / 1000);
	//   printf("Overall messages-per-second: %g\n",
	  //        (message_count / (total_delta / 1000000.0)));
  }
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

int send_msg(char const *hostname, int port, int message_count) {
  int  status;
  amqp_socket_t *socket = NULL;
  amqp_connection_state_t conn;
  /*
  if (argc < 5) {
    fprintf(stderr,
            "Usage: amqp_producer host port rate_limit message_count\n");
    return 1;
  }

  hostname = argv[1];
  port = atoi(argv[2]);
  rate_limit = atoi(argv[3]);
  message_count = atoi(argv[4]);
  */

  conn = amqp_new_connection();

  socket = amqp_tcp_socket_new(conn);
  if (!socket) {
    die("creating TCP socket");
  }

  status = amqp_socket_open(socket, hostname, port);
  if (status) {
    die("opening TCP socket");
  }
  die_on_amqp_error(amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest"), "Logging in");
  amqp_channel_open(conn, 1);
  die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel");

  amqp_bytes_t  queue = amqp_cstring_bytes ("hello");
  amqp_queue_declare_ok_t*  rc_decl = amqp_queue_declare (conn, 1, queue, 0, 0, 0, 0, amqp_empty_table);

  send_batch(conn, "hello",  message_count);

  die_on_amqp_error(amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS),
                    "Closing channel");
  die_on_amqp_error(amqp_connection_close(conn, AMQP_REPLY_SUCCESS),
                    "Closing connection");
  die_on_error(amqp_destroy_connection(conn), "Ending connection");
  return 0;
}

int main(int argc, char const *const *argv) {
  int i, flag = 1;
  char *shared_memory;
  shared_memory = setup_shmem();
  char start_input = getchar();

  if (argc < 4) {
    fprintf(stderr,
            "Usage: amqp_producer host port message_count\n");
    return 1;
  }

  for (i = 0; i < 10; i++) {
	while (flag == 0) {
	  memcpy(&flag, shared_memory, sizeof(flag));
	  sleep(1);
	}
	flag = 0;
	memcpy(shared_memory, &flag, sizeof(flag));
	send_msg(argv[1], atoi(argv[2]), atoi(argv[3]));
	sleep(5);
  }
  return 0;
}
