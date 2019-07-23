clean: 	
	rm server;
	rm client;

all:
	gcc -g -pthread UDP_Client.c -o client;
	gcc -g -pthread UDP_Server.c -o server;