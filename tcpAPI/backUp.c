#include <stdio.h>
#include <stdlib.h>	/* needed for os x */
#include <string.h>	/* for memset */
#include <sys/socket.h>
#include <arpa/inet.h>	/* for printing an internet address in a user-friendly way */
#include <netinet/in.h>
#include <sys/errno.h>   /* defines ERESTART, EINTR */
#include <sys/wait.h>    /* defines WNOHANG, for wait() */
#include <sys/types.h> 
#include <netdb.h>
#include <unistd.h>
#include "common.h"
#ifndef ERESTART
#define ERESTART EINTR
#endif


extern int errno;


void serve(int port);	/* main server function */

int main(int argc, char **argv)
{
	extern char *optarg;
	extern int optind;
	int c, err = 0; 
	int port = SERVER_PORT;
	static char usage[] = "usage: %s [-d] [-p port]\n";

	while ((c = getopt(argc, argv, "dp:")) != -1)
		switch (c) {
		case 'p':
			port = atoi(optarg);
			if (port < 1024 || port > 65535) {
				fprintf(stderr, "invalid port number: %s\n", optarg);
				err = 1;
			}
			break;
		case '?':
			err = 1;
			break;
		}
	if (err || (optind < argc)) {
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}
	serve(port);
return 0;
}

/* serve: set up the service */

void
serve(int port)
{
	int svc;        /* listening socket providing service */
	int rqst;       /* socket accepting the request */
	socklen_t alen;       /* length of address structure */
	struct sockaddr_in my_addr;    /* address of this service */
	struct sockaddr_in client_addr;  /* client's address */
	int sockoptval = 1;
	pid_t pid;
	char hostname[128]; /* host name, for debugging */
	char send_buff[BUFFER_SIZE]; //buffer
	char recv_buff[BUFFER_SIZE]; //buffer
	bzero(send_buff,BUFFER_SIZE);
	bzero(recv_buff,BUFFER_SIZE);
	char * tmp, * ptr;

	gethostname(hostname, 128);


	if ((svc = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("cannot create socket");
		exit(1);
	}


	setsockopt(svc, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));
    int sock, flag, ret;


	memset((char*)&my_addr, 0, sizeof(my_addr));  /* 0 out the structure */
	my_addr.sin_family = AF_INET;   /* address family */
	my_addr.sin_port = htons(port);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(svc, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
		perror("bind failed");
		exit(1);
	}

	/* set up the socket for listening with a queue length of 5 */
	if (listen(svc, 5) < 0) {
		perror("listen failed");
		exit(1);
	}

	printf("server started on %s, listening on port %d\n", hostname, port);

	/* loop forever - wait for connection requests and perform the service */
	alen = sizeof(client_addr);     /* length of address */

	while(1) {
		if ((rqst = accept(svc,
		                (struct sockaddr *)&client_addr, &alen)) < 0) {
			if ((errno != ECHILD) && (errno != ERESTART) && (errno != EINTR)) {
				perror("accept failed");
				exit(1);
			}
		}
		pid=fork();
		if (pid < 0) {
			perror("ERROR in new process creation");
		}

		if(pid==0) {
		if ( recv(rqst, recv_buff, BUFFER_SIZE, 0) < 0 )
            perror("fail to accept\n");

		printf("received a connection from: %s port %d\n",
		inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        printf("received message: %s.\n", recv_buff);
        strcpy(send_buff, "hello to you too");

        if(send(rqst, send_buff, sizeof(send_buff), 0) < 0 ){
            perror("fail to send.");
        }
		close(rqst);

		}
		else
		close(rqst);

	}
}
