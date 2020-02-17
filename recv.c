#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <amqp_framing.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "utils.h"

#define CLOCK_HZ 2600000000.0
#define MAX_BUF_SIZE 1024
#define rdtsc_64(lower, upper) asm __volatile ("rdtsc" : "=a"(lower), "=d" (upper));

void connection_close(amqp_connection_state_t* conn, amqp_channel_t channel);

void  callback (amqp_connection_state_t* conn, amqp_envelope_t* envelope, amqp_bytes_t body) {
  printf (" [x] Received ");
  fwrite (body.bytes, 1, body.len, stdout);
  printf ("\n");
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

int  recv_msg (int max_count) {
  int count = 0;
  int i;
  unsigned int tsc_l, tsc_u; //uint32_t
  unsigned long int log_tsc[2][max_count];
  amqp_connection_state_t  conn = amqp_new_connection ();

  amqp_socket_t*  socket = amqp_tcp_socket_new (conn);

  int  rc_sock = amqp_socket_open (socket, "localhost", 5672);
  if (rc_sock != AMQP_STATUS_OK) {
	fprintf (stderr, "connection failure.\n");
	exit (1);
  }

  amqp_rpc_reply_t  rc_login = amqp_login (conn, "/", AMQP_DEFAULT_MAX_CHANNELS, AMQP_DEFAULT_FRAME_SIZE, AMQP_DEFAULT_HEARTBEAT, AMQP_SASL_METHOD_PLAIN, "guest", "guest");
  if (rc_login.reply_type != AMQP_RESPONSE_NORMAL) {
	if (rc_login.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION) {
	  if (rc_login.reply.id == AMQP_CONNECTION_CLOSE_METHOD) {
		amqp_connection_close_t *m = (amqp_connection_close_t *) rc_login.reply.decoded;
		fwrite (m->reply_text.bytes, 1, m->reply_text.len, stderr);
		fprintf (stderr, "\n");
	  }
	}
	exit (1);
  }

  amqp_channel_t  channel = 1;
  amqp_channel_open_ok_t*  rc_channel = amqp_channel_open (conn, channel);

  amqp_bytes_t  queue = amqp_cstring_bytes ("hello");
  amqp_queue_declare_ok_t*  rc_decl = amqp_queue_declare (conn, channel, queue, false, false, false, false, amqp_empty_table);

  amqp_basic_consume_ok_t*  rc_cons = amqp_basic_consume (conn, channel, queue, amqp_empty_bytes, false, true/*no_ack*/, false, amqp_empty_table);

  //printf (" [*] Waiting for messages. To exit press CTRL+C\n");

  while (count < max_count) {
	//	while(1) {
	//	printf("count:%d, max_count:%d\n",count,max_count);
	amqp_maybe_release_buffers (conn);
	amqp_envelope_t  envelope;
	amqp_rpc_reply_t  rc_msg = amqp_consume_message (conn, &envelope, NULL, 0);
	switch (rc_msg.reply_type) {
	case AMQP_RESPONSE_NORMAL:
	  //callback (&conn, &envelope, envelope.message.body);
	  memcpy(&log_tsc[0][count], envelope.message.body.bytes, sizeof(unsigned long int));
		rdtsc_64(tsc_l, tsc_u);
		log_tsc[1][count] = (unsigned long int)tsc_u<<32 | tsc_l;

	  break;
	case AMQP_RESPONSE_LIBRARY_EXCEPTION:
	  if (rc_msg.library_error == AMQP_STATUS_UNEXPECTED_STATE) {
		amqp_frame_t  frame;
		if (amqp_simple_wait_frame (conn, &frame) != AMQP_STATUS_OK) {
		  connection_close(conn, channel);
		}
		if (frame.frame_type == AMQP_FRAME_METHOD) {
		  switch (frame.payload.method.id) {
		  case AMQP_BASIC_ACK_METHOD:
			break;
		  case AMQP_BASIC_RETURN_METHOD: {
			amqp_message_t  message;
			amqp_rpc_reply_t  rc_read = amqp_read_message (conn, frame.channel, &message, 0);
			if (rc_read.reply_type != AMQP_RESPONSE_NORMAL) {
			  connection_close(conn, channel);
			}
			amqp_destroy_message (&message);
			break;
		  }
		  case AMQP_CHANNEL_CLOSE_METHOD:
		  case AMQP_CONNECTION_CLOSE_METHOD:
			connection_close(conn, channel);
			break;
		  default:
			fprintf (stderr ,"An unexpected method was received %d\n", frame.payload.method.id);
			connection_close(conn, channel);
		  }
		}
	  }
	  break;
	default:;
	}
	amqp_destroy_envelope (&envelope);
	count++;
  }
  connection_close(conn, channel);

  unsigned long int start = log_tsc[0][0];

  for (i = 0; i < max_count; i++) {
	printf("%lf, %lf, %lf\n",
		   (log_tsc[0][i] - start) / CLOCK_HZ,
		   (log_tsc[1][i] - start) / CLOCK_HZ,
		   (log_tsc[1][i] - log_tsc[0][i]) / CLOCK_HZ);
  }

  return 0;
}

void connection_close(amqp_connection_state_t *conn, amqp_channel_t channel)
{
  amqp_rpc_reply_t  rc_chclose = amqp_channel_close (conn, channel, AMQP_REPLY_SUCCESS);
  amqp_rpc_reply_t  rc_conclose = amqp_connection_close (conn, AMQP_REPLY_SUCCESS);
  amqp_destroy_connection (conn);
}

int main(int argc, char *argv[])
{
  char *shared_memory;
  int i, flag = 1;
  shared_memory = setup_shmem("test.dat", 53);
  for (i = 0; i < 10; i++) {
	recv_msg(atoi(argv[1]));
	memcpy(shared_memory, &flag, sizeof(flag));
  }
  return 0;
}
