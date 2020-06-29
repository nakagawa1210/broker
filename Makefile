all: send_s recv_s send_c recv_c send recv

#mybroker

send_s: send_server.c
	gcc send_server.c -o send_server -std=gnu99
recv_s: recv_server.c
	gcc recv_server.c -o recv_server -std=gnu99
send_c: send_client.c
	gcc send_client.c -o send_client -std=gnu99
recv_c: recv_client.c
	gcc recv_client.c -o recv_client -std=gnu99


#rabbitmq

send: send.c utils.c platform_utils.c
	gcc -I./include -L./x86_64-linux-gnu -o send send.c -lrabbitmq -Wl,-rpath,./x86_64-linux-gnu -Wl,-rpath,./lib utils.c platform_utils.c
recv: recv.c platform_utils.c
	gcc -I./include -L./x86_64-linux-gnu -o recv recv.c -lrabbitmq -Wl,-rpath,./x86_64-linux-gnu -Wl,-rpath,./lib platform_utils.c
