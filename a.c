/*
	Williams College
	Computer Science 339- Distributed Systems
	Assignment 1: Building a Web Server
	Authors: Nile Livingston and Juan Mena
	
	The code below is (currently) heavily borrowed from the example program
	server.c provided by the paper "Berkeley UNIX System Calls and Interprocess
	Communication", written by Lawrence Besaw and revised by Marvin Solomon.

*/

#include <stdio.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

// input: 1 command line argument specifying the port # of the server.
int main(int argc, char** argv) {

	struct servent *serv_pointer; // server entry struct: keeps track of entries in the services database
	struct sockaddr_in server, remote; // 
	int request_sock, new_sock;
	int nfound, fd, maxfd, bytesread, addrlen;
	fd_set rmask, mask;
	static struct timeval timeout = { 0, 500000 }; // 0.5 second timeout for the select() syscall.

	// Improper # of input args; call error.
	if (argc != 2) {
		(void) fprintf(stderr, "usage: %s port_number\n", argv[0]);
		exit(1);
	}
	// Attempt to establish socket and report error if necessary.
	if ((request_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("Error in requesting socket.");
		exit(1);
	}
	// If port number is valid, construct servent and assign port number.
	if (isdigit(argv[1][0])) {
		static struct servent s;
		serv_pointer = &s;
		s.s_port = htons((u_short)atoi(argv[1]));
	}
	// Get the pointer to the service at the input port and report error if none found. 
	else if ((serv_pointer = getservbyname(argv[1], "tcp")) == 0) {
		fprintf(stderr, "%s: unknown service\n", argv[0]);
		exit(1);
	}

	// Set up the sockaddr_in server.
	memset((void *) &server, 0, sizeof server);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = serv_pointer->s_port;

	// Attempt to bind the socket and report error if necessary.
	if (bind(request_sock, (struct sockaddr *)&server, sizeof server) < 0) {
        perror("Error in binding socket.");
		exit(1); 
	}
	// Attempt to set up listening on the socket and report error if necessary.
	if (listen(request_sock, 10) < 0) {
        perror("Error in attempting to listen.");
        exit(1);
    }

    // ???
    FD_ZERO(&mask);
    FD_SET(request_sock, &mask);
    maxfd = request_sock;
    
    // Main loop: scan for new data, accept incoming connections if possible,
    //			  spawn off new threads for each request.
    while (1) {
    	rmask = mask;
    	nfound = select(maxfd+1, &rmask, (fd_set *)0, (fd_set *)0, &timeout);
    	// If some error or interrupt occurs with select.
    	if (nfound < 0) {
    		// Some signal was caught.
            if (errno == EINTR) {
                printf("Interrupted syscall.\n");
                continue;
    		}
    		perror("Error in select().");
    		exit(1);
    	}
    	// A timeout occurs.
    	if (nfound == 0) {
    		printf("."); fflush(stdout);
    		continue;
    	}

    	// A connection is available.
    	if (FD_ISSET(request_sock, &rmask)) {
    		addrlen = sizeof(remote);
    		new_sock = accept(request_sock, (struct sockaddr *)&remote, &addrlen);
    		// Something went wrong with accepting; report error and break.
    		if (new_sock < 0) {
        		perror("Error in accepting connection.");
				exit(1); 
			}
    		printf("Connection from host %s, port %d, socket %d\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port),new_sock);
    		FD_SET(new_sock, &mask);
    		if (new_sock > maxfd) {
        		maxfd = new_sock;
        	}
    		FD_CLR(request_sock, &rmask);
    	}
    }		
}