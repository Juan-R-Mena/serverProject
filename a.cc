/*
	Williams College
	Computer Science 339- Distributed Systems
	Assignment 1: Building a Web Server
	Authors: Nile Livingston and Juan Mena
	
	The code below is (currently) heavily borrowed from the example program
	server.c provided by the paper "Berkeley UNIX System Calls and Interprocess
	Communication", written by Lawrence Besaw and revised by Marvin Solomon.

*/

#define _OE_SOCKETS
#include <sys/types.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <regex>


char* stringToCharArray(std::string input) {
	char *a = new char[input.size()+1];
	a [input.size()] = 0;
	memcpy(a, input.c_str(), input.size());
	return a;
}

// Return 1 if input is validly-formed GET request, otherwise return 0.
int isValidGetRequest (std::string method, std::string path, std::string protocol) {
	// Parse get command.
	int get_right = method.compare("GET ") == 0;

	// FIX REGEX.
	std::regex e ("/(\\w|\\.)+(/(\\w|)*)*|/");
	int path_right = std::regex_match (path, e);

	int protocol_right = (protocol.compare("HTTP/1.0") == 0) || (protocol.compare("HTTP/1.1") == 0);

	return get_right && path_right && protocol_right;
}  

// Parse and return path.
std::string getPath(std::string input) {
	// Parse path.
	int i = 3;
	int parsing = 1;
	int start, len = 0;
	int counting = 0;
	while (parsing) {
		// We're looking for the beginning of the path.
		if (input[i] != ' ') {
			if (!counting) {
				start = i;
				counting = 1;
				len++;
			}
			else {
				len++;
			}
		}
		else {
			if (counting) break;
		}
		i++;
	}
	return input.substr(start, len);
}

// Parse and return protocol or INVALID.
std::string getProtocol(std::string input) {
	std::string protocol = "";
	if ((input.find("HTTP/1.0\n\n")) != std::string::npos) {
		return "HTTP/1.0";
	}
	else if ((input.find("HTTP/1.1\n\n")) != std::string::npos) {
		return "HTTP/1.1";
	}
	else {
		return "INVALID";
	}
}

// input: 1 command line argument specifying the port # of the server.
int main(int argc, char** argv) {

	/*
	// get data into string form.
	std::string input = "GET /index.html HTTP/1.1\n\n";
	// parse the string
	std::string method = input.substr(0, 4);
	std::string path = getPath(input);
	std::string protocol = getProtocol(input);

	std::cout << method << path << ' ' << protocol << '\n';

	// verify valid form
	if (isValidGetRequest(method, path, protocol)) {
		printf("YAY!\n");
	}
	else {
		// Return 400
		printf("ERR 400");
	}c
	return 0;
	*/

	struct servent *serv_pointer; // server entry struct: keeps track of entries in the services database
	struct sockaddr_in server, remote; // main server address and sender address
	int request_sock, new_sock; // the first is used to listen, whereas the new socket created by accept() is stored in the second.
	int nfound, fd, maxfd, bytesread;
	int addrlen;
	fd_set rmask, mask;
	static struct timeval timeout = { 0, 500000 }; // 0.5 second timeout for the select() syscall.
	char buf[BUFSIZ];

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
     		new_sock = accept(request_sock, (struct sockaddr *)&remote, (socklen_t *) &addrlen);
  
    		// Something went wrong with accepting; report error and break.
    		if (new_sock < 0) {
        		perror("Error in accepting connection");
				exit(1); 
			}
    		printf("Connection from host %s, port %d, socket %d\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port),new_sock);
    		FD_SET(new_sock, &mask);
    		if (new_sock > maxfd) {
        		maxfd = new_sock;
        	}
    		FD_CLR(request_sock, &rmask);
    	}

    	// Process incoming data
    	for (fd=0; fd <= maxfd ; fd++) {
    	/* look for other sockets that have data available */
    	if (FD_ISSET(fd, &rmask)) {
			/* process the data */
			bytesread = read(fd, buf, sizeof buf - 1);
			if (bytesread<0) {
    			perror("Error in reading data.");
    			/* fall through */
			}
			if (bytesread<=0) {
    			printf("Server: end of file on %d\n",fd);
    			FD_CLR(fd, &mask);
    			if (close(fd)) perror("Error in closing connection.");
    			continue;
			}
			buf[bytesread] = '\0';
			printf("%s: %d bytes from %d: %s\n", argv[0], bytesread, fd, buf);

			// get data into string form.
			std::string input = "GET /path HTTP/1.0\n\n";
			// parse the string
			std::string method = input.substr(0, 4);
			std::string path = getPath(input);
			std::string protocol = getProtocol(input);

			// verify valid request form.
			if (isValidGetRequest(method, path, protocol)) {
				printf("YAY!");
				// verify file exists
				/*if (...) {}
				else {
					// Return 404
					out = stringToCharArray("404 Not Found")
					write(fd, out, sizeof(out))
					close(new_sock);
					break;
				}*/

				// verify file permissions
				/*
				if (...) {}
				else{
					// Return 403
					out = stringToCharArray("403 Forbidden")
					write(fd, out, sizeof(out))
					close(new_sock);
					break;
				}*/

				// All's well, return 200 OK and send file.
				/* 


				// Close the connection if HTTP 1.0
				if (protocol.compare("HTTP/1.0") == 0) {
					out = stringToCharArray("200 OK")
					write(fd, out, sizeof(out))
					close(new_sock);
				}
				*/ 
			}
			else {
				// Return 400
				/*
				close(new_sock);
				*/
			}

			/* echo it back */
			if (write(fd, buf, bytesread)!=bytesread)
    			perror("Error in echoing");
    		}
    	}
    }		
}

