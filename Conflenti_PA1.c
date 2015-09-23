/*
 * Steven Conflenti
 * CSCI 4273
 * PA1: HTTP Web-Based Server
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>      
#include <strings.h>     
#include <unistd.h>     
#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

/* struct configurations stores information parsed from ws.conf inside the global config variable */
struct config {
  int portNum;
  char rootDir[512];
  char indices[8][64]; // Allows for up to 8 indices of string length <= 64
  int indicesCount;
  char contentTypes[16][64]; // Allows for up to 16 content types of string length <= 64
  char contentStrings[16][64];
  int contentTypesCount;
} config;

/* struct which will be instantiated for requests from a client connection */
struct client_request {
	int keepAlive; // Boolean representing whether or not to keep alive
	char method[256];
	char uri[256];
	char version[256];
	size_t req_len;
};

/* struct which will be instantiated when our server is building a response to send to the client */
struct server_response {
	char message[4096];
	char type[128];
	char length[128];
};

int openSock(int portNum);

int parseConfig(const char* confName);

void handleRequest(int connectionfd);

void buildResponse(int connectionfd, struct client_request request, struct server_response response);

int checkValid(int connectionfd, struct client_request request, struct server_response response);

// Start server
int main(int argc, char **argv) {

	// Parse the information from ws.conf file.
 	if (parseConfig("./ws.conf") == -1) {
 		printf("Unable to start server.\n");
 		exit(EXIT_FAILURE);
 	}

 	// Create a file descriptor for our server socket.
	int listenfd;
 	if ((listenfd = openSock(config.portNum)) < 0) {
 		exit(EXIT_FAILURE);
 	} 
 	else { // If server successfully starts, listen on port for incoming client requests.
 		int connectionfd;
		struct sockaddr_in clientAddress;
		int clientLength = sizeof(struct sockaddr_in);

		while (1) {
			// Wait for a client to send a request to access the server.
			connectionfd = accept(listenfd, (struct sockaddr*)&clientAddress, &clientLength);
			if (connectionfd) {
				// If fork() returns 0 it is the child process and will handle the requests from the client. 
				// The parent process can keep listening for other clients trying to connect.
				if (!fork()) {
					handleRequest(connectionfd);
					exit(0);
				}
			}
			close(connectionfd);
		}
 	}

 	close(listenfd);

 	return 0;
}

int parseConfig(const char* confName) {
  FILE *conf_fd;
  size_t len = 0;
  int readLength = 0;
  char buffer1[256];
  char buffer2[256];
  char *currentLine;
  config.contentTypesCount = 0;

  // If ws.conf exists, open it
  printf("\nParsing Configuration File...\n");
  if (!(conf_fd = fopen(confName, "r"))) {
    fprintf(stderr, "Unable open configuration file.\n");
    return -1;
  }

  // Iterate through lines of ws.conf file.
  while ((readLength = getline(&currentLine, &len, conf_fd)) != -1) {

  	// End each line with a null terminated character so we can strcpy and iterate properly.
  	currentLine[readLength-1] = '\0';

  	// Skip if line is blank or a comment line.
    if (currentLine[0] == '#' || currentLine[0] == '\0') {
      continue;
    }

    sscanf(currentLine, "%s %s", buffer1, buffer2);

    if (!strcmp(buffer1, "Listen")) {   	

      config.portNum = atoi(buffer2); // Cast the port number to an int.
      if (config.portNum > 65535 || config.portNum < 2) {
      	printf("Invalid port number.\n");
      	return -1;
      }
      printf("Port number: %d\n", config.portNum);

    } else if (!strcmp(buffer1, "DocumentRoot")) {

    	memcpy(config.rootDir, &buffer2[1], strlen(buffer2) - 2);
    	printf("Document root: %s\n", config.rootDir);

    } else if (!strcmp(buffer1, "DirectoryIndex")) {
    	char *strOut; // extracted string
    	int index = 0;
      	
      	// strtok will extract each token until there are none left
    	strOut = strtok(currentLine, " ");
    	strOut = strtok(NULL, " ");

    	while (strOut != NULL) {
    		printf("Extracted index: %s\n", strOut);
    		strcpy(config
    		.indices[index++], strOut);
    		strOut = strtok(NULL, " ");
    	}

    	config.indicesCount = index;

    } else if (buffer1[0] == '.') {
    	printf("Encountered content type: %s\n", buffer1);
    	strcpy(config.contentTypes[config.contentTypesCount], buffer1);
    	strcpy(config.contentStrings[config.contentTypesCount++], buffer2);
    }
  }

  printf("Finished parsing configuration file. Starting server...\n\n");
  fclose(conf_fd);
  if (currentLine) {
    free(currentLine);
  }
  return 1;
}

// Opens and returns a socket which is listening on a port that is obtained from ws.conf.
int openSock(int portNum) {
  int listenfd;
  int reuse = 1; // This value is used as a boolean to indicate the option of reuse port if it is not currently active.
  struct sockaddr_in server_address;

  // Creates and assigns the socket to listenfd
  // Socket created will use IPv4 protocols specified by AF_INET
  // SOCK_STREAM parameter specifies to used sequenced, two-way, connection-based byte streams.
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
    fprintf(stderr, "Unable to listen to port %d", portNum);
    return -1;
  } else {
    printf("Socket successfully created. Listening on port %d...\n\n", portNum);
  }
  // Specifies we should allow reuse of local ports if it is not active, we can rebind
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&reuse , sizeof(int)) < 0) {
    fprintf(stderr, "Could not unbind socket that is already bound.");
        return -1;
  }
  server_address.sin_family = AF_INET; 
  server_address.sin_addr.s_addr = htonl(INADDR_ANY); 
  server_address.sin_port = htons((unsigned short)portNum); 

  // Binds server to the socket which can be accessed by its identifier, the port
  if (bind(listenfd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
    fprintf(stderr, "Unable to bind server to the socket.");
      return -1;
  }

  // The socket will now be listening and ready to accept connection requests 
  if (listen(listenfd, 1024) < 0) { // listen() takes socket, backlog as parameters 
    fprintf(stderr, "Socket could not begin listening for incoming requests.");
    return -1;
  }

  return listenfd;
}

void handleRequest(int connectionfd) {
	struct server_response response;
  	struct client_request request;
  	char requestString[8192];

  	// Assign an fd_set to our client socket so we can monitor it and have a timer
  	fd_set set;
  	FD_ZERO(&set);
  	FD_SET(connectionfd, &set);

  	// Set up timer
  	struct timeval time;
  	time.tv_sec = 10;
  	int status;

  	// Starts a timer for this connection and begins waiting for any type of input.
  	// If input is received without keep-alive or the connection is not refreshed it's closed.
  	while ((status = select(connectionfd + 1, &set, NULL, NULL, &time)) > 0) {

  		// If we are notified above that there is io waiting, begin reading the request.
	  	int waitingRequest = recv(connectionfd, &requestString, 4096, 0);

	  	if (waitingRequest != 0) {

	  		memset(&request.method[0], 0, sizeof(request.method));
		  	memset(&request.uri[0], 0, sizeof(request.uri));
		  	memset(&request.version[0], 0, sizeof(request.version));

			sscanf(requestString, "%s %s %s %*s", request.method, request.uri, request.version);

			// Make sure our request is valid.
			int valid = checkValid(connectionfd, request, response);

			if (valid == 1) { // Invalid requests should never need to be kept alive.
				if (strstr(requestString, "Connection: keep-alive")) {
					// If it says keep-alive we will refresh our timer for the connection.
			  		time.tv_sec = 10;
			  		request.keepAlive = 1;
			  	} else {
			  		// If it does not have keep-alive close the connection.
			  		time.tv_sec = 0;
			  		request.keepAlive = 0;
			  	}

				buildResponse(connectionfd, request, response);
			} // That being said, invalid requests don't terminate the connection immediately.

		  	memset(&requestString[0], 0, sizeof(requestString));
		}
	}

	printf("Client connection closed.\n");
	close(connectionfd);
}


void buildResponse(int connectionfd, struct client_request request, struct server_response response) {
	int pathLength = strlen(config.rootDir) + strlen(request.uri);
	char fullpath[pathLength];
	strcpy(fullpath, config.rootDir);
	strcat(fullpath, request.uri);

	int fullpathExists = 0;
	int indexExists = 0;
	FILE *fullpathfd;

	// Handle if it's a desired index
	if (!(strcmp(request.uri, "/"))) {
		// Loop through our possible indices to see if desired indice exists
		int i;
		for (i = 0; i < config.indicesCount; i++) {
			char tempFullpath[pathLength + strlen(config.indices[i])];

			strcpy(tempFullpath, fullpath);
			strcat(tempFullpath, config.indices[i]);

			fullpathfd = fopen(tempFullpath, "rb"); // rb for non-text files
			if (fullpathfd) {
				fullpathExists = 1;
				indexExists = 1;
				break;
			}

		}

	} else { // They asked for a specific file
		fullpathfd = fopen(fullpath, "rb");
		if (fullpathfd) {
			fullpathExists = 1;
		}
	}

	if (!fullpathExists) {
		// If the path (URI) they requested isn't found -> 404 error
		memset(&response.message[0], 0, sizeof(response.message));
		strcpy(response.message, "HTTP/1.1 404 Not Found: ");
		strcat(response.message, request.uri);
		strcat(response.message, "\n\n");
		send(connectionfd, response.message, sizeof(response.message), 0);
	} else {
		// Get the content type of the file
		char* filetype;
		int i;
		for (i = 0; i < config.contentTypesCount; i++) {
			if (strstr(request.uri, config.contentTypes[i])) { // returns first occurance of the contect type in the uri
				filetype = config.contentStrings[i];
			}
		}

		memset(&response.message[0], 0, sizeof(response.message));
		memset(&response.type[0], 0, sizeof(response.type));
		memset(&response.length[0], 0, sizeof(response.length));

		// If filetype is null -> 501 error
		if (filetype == NULL && indexExists == 0) { // File type not supported and it is not the index
			strcpy(response.message, "HTTP/1.1 501 Not Implemented: ");
			strcat(response.message, request.uri);
			strcat(response.message, "\n\n");
			printf("INFO: Status of request - Error Encountered, 501\n\n");
			send(connectionfd, response.message, sizeof(response.message), 0);
			return;
		}

		strcpy(response.message, "HTTP/1.1 200 OK\n");

		if (indexExists == 0) {
			sprintf(response.type, "Content-Type: %s\n", filetype);
			printf("Content-Type: %s\n", filetype);
		} else { // This triggers if it is the index page.
			sprintf(response.type, "Content-Type: %s", "text/html\n");
			printf("Content-Type: text/html\n");
		}

		// Gets content length by seeking to the end andfinding the current index.
		fseek(fullpathfd, 0, SEEK_END);
		int size = ftell(fullpathfd);
		sprintf(response.length, "Content-Length: %d\n", size);
		
		char res[8192];

		// Construct the response and send it back to the client.
		strncpy(res, response.message, sizeof(response.message));
		strcat(res, response.type);
		strcat(res, response.length);
		if (request.keepAlive == 1) {
			strcat(res, "Connection: keep-alive\n");
		}
		strcat(res, "\n");

		send(connectionfd, res, strlen(res), 0);

		fseek(fullpathfd, 0, SEEK_SET);
		size_t read = 0;
		char *fileContents;
		fileContents = malloc(8192);

		// Read and begin writting the file reqested by the client to the client
		while ((read = fread(fileContents, 1, 8192, fullpathfd)) > 0) {
			write(connectionfd, fileContents, (int)read);
		}
		
		send(connectionfd, "\n", 2, 0);

		free(fileContents);
		fclose(fullpathfd);
	}

	if (fullpathExists == 1) {
		printf("INFO: Status of request - URI Found\n\n");
	} else {
		printf("INFO: Status of request - URI Not Found - Error Encountered, 404\n");
		printf("URI Requested: %s\n\n", request.uri);
	}
}

/*
 * Responsible for checking if the Request has valid parameters for method, uri, and version
 * We only support GET for our server as per the e-mail instructions, anything else is
 * not supported.
 */
int checkValid(int connectionfd, struct client_request request, struct server_response response) {

	memset(&response.message[0], 0, sizeof(response.message));

	// Checks Method Validity
	if (strcmp(request.method, "GET") != 0) {
		strcpy(response.message, "HTTP/1.1 400 Bad Request: Invalid Method: ");
		strcat(response.message, request.method);
		strcat(response.message, "\n\n");
		printf("INFO: Status of request - Error Encountered, 401\n\n");
		send(connectionfd, response.message, sizeof(response.message), 0);
		return 0;
	}

	// Checks URI Validity
	if (request.uri[0] == '\0') {
		strcpy(response.message, "HTTP/1.1 400 Bad Request: Invalid URI: ");
		strcat(response.message, request.uri);
		strcat(response.message, "\n\n");
		printf("INFO: Status of request - Error Encountered, 402\n\n");
		send(connectionfd, response.message, sizeof(response.message), 0);
		return 0;
	}

	//Checks Version Validity
	if (!(strcmp(request.version, "HTTP/1.1") == 0 || strcmp(request.version, "HTTP/1.2") == 0)) {
		strcpy(response.message, "HTTP/1.1 400 Bad Request: Invalid HTTP-Version: ");
		strcat(response.message, request.version);
		strcat(response.message, "\n\n");
		printf("INFO: Status of request - Error Encountered, 403\n\n");
		send(connectionfd, response.message, sizeof(response.message), 0);
		return 0;
	}

	return 1;
}
