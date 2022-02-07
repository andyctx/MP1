#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "interface.h"


/*
 * TODO: IMPLEMENT BELOW THREE FUNCTIONS
 */
int connect_to(const char *host, const int port);

struct Reply process_command(const int sockfd, char* command, char** argv);

void process_chatmode(const char* host, const int port);

void* client_helper(void* args);

struct Server {
	char* host;
	int port;
};

int main(int argc, char** argv)
{
	int sockfd;
	if (argc != 3) {
		fprintf(stderr, "usage: enter host address and port number\n");
		exit(1);
	}

	display_title();

	while (1) {
		
		sockfd = connect_to(argv[1], atoi(argv[2]));

		if(sockfd < 0) {
			perror("Connection failed");
			exit(EXIT_FAILURE);
		}

		char command[MAX_DATA];
  		get_command(command, MAX_DATA);
		
		struct Reply reply = process_command(sockfd, command, argv);
		display_reply(command, reply);

		// printf("reply: %i\n", reply.status);

		if(reply.status > 0) {
			continue;
		}
		
		touppercase(command, strlen(command) - 1);
		if (strncmp(command, "JOIN", 4) == 0) {
			printf("Now you are in the chatmode\n");
			process_chatmode(argv[1], reply.port);
		}
	}
	close(sockfd);
	return 0;
}

/*
 * Connect to the server using given host and port information
 *
 * @parameter host    host address given by command line argument
 * @parameter port    port given by command line argument
 *
 * @return socket fildescriptor
 */
int connect_to(const char *host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE :
	// In this function, you are suppose to connect to the server.
	// After connection is established, you are ready to send or
	// receive the message to/from the server.
	//
	// Finally, you should return the socket fildescriptor
	// so that other functions such as "process_command" can use it
	// ------------------------------------------------------------

	int sockfd;
	struct sockaddr_in server;
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Socket creation error\n");
  	return -1;
	}

	server.sin_family = AF_INET;
	server.sin_port = htons( port );
	server.sin_addr.s_addr = inet_addr( host );
	
	char pbuf[INET_ADDRSTRLEN];
	
	inet_ntop(AF_INET, &(server.sin_addr.s_addr), pbuf, INET_ADDRSTRLEN);
	
	// printf("Host: %s\nPort: %i\n", pbuf, ntohs(server.sin_port));

	if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		printf("connect error\n");
		return -1;
	}
	// printf("Connected to server\n");

	return sockfd;
}

/*
 * Send an input command to the server and return the result
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 * @parameter command  command will be sent to the server
 *
 * @return    Reply
 */
struct Reply process_command(const int sockfd, char* command, char** argv)
{
	struct Reply reply;
	char* sreply[2000];
	
	reply.status = FAILURE_UNKNOWN;
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse a given command
	// and create your own message in order to communicate with
	// the server. Surely, you can use the input command without
	// any changes if your server understand it. The given command
  // will be one of the followings:
	//
	// CREATE <name>
	// DELETE <name>
	// JOIN <name>
  // LIST
	//
	// -  "<name>" is a chatroom name that you want to create, delete,
	// or join.
	//
	// - CREATE/DELETE/JOIN and "<name>" are separated by one space.
	// ------------------------------------------------------------

	// ------------------------------------------------------------
	// GUIDE 2:
	// After you create the message, you need to send it to the
	// server and receive a result from the server.
	// ------------------------------------------------------------
	
	// Prevent empty input
	if(strcmp(command, "") == 0) {
		perror("Empty input");
		reply.status = FAILURE_INVALID;
		return reply;
	}
	
	// Send command to server
	if(send(sockfd, command, strlen(command), 0) < 0) {
		perror("Send failed");
		reply.status = FAILURE_UNKNOWN;
		return reply;
	}
	
	// Read server reply
    if(recv(sockfd, sreply , 2000 , 0) < 0) {
		perror("recv failed");
		reply.status = FAILURE_UNKNOWN;
		return reply;
	}

	reply = *((struct Reply*)sreply);

	// ------------------------------------------------------------
	// GUIDE 3:
	// Then, you should create a variable of Reply structure
	// provided by the interface and initialize it according to
	// the result.
	//
	// For example, if a given command is "JOIN room1"
	// and the server successfully created the chatroom,
	// the server will reply a message including information about
	// success/failure, the number of members and port number.
	// By using this information, you should set the Reply variable.
	// the variable will be set as following:
	//
	// Reply reply;
	// reply.status = SUCCESS;
	// reply.num_member = number;
	// reply.port = port;
	//
	// "number" and "port" variables are just an integer variable
	// and can be initialized using the message fomr the server.
	//
	// For another example, if a given command is "CREATE room1"
	// and the server failed to create the chatroom becuase it
	// already exists, the Reply varible will be set as following:
	//
	// Reply reply;
	// reply.status = FAILURE_ALREADY_EXISTS;
  //
  // For the "LIST" command,
  // You are suppose to copy the list of chatroom to the list_room
  // variable. Each room name should be seperated by comma ','.
  // For example, if given command is "LIST", the Reply variable
  // will be set as following.
  //
  // Reply reply;
  // reply.status = SUCCESS;
  // strcpy(reply.list_room, list);
  //
  // "list" is a string that contains a list of chat rooms such
  // as "r1,r2,r3,"
	// ------------------------------------------------------------ 

	return reply;
}

/*
 * Get into the chat mode
 *
 * @parameter host     host address
 * @parameter port     port
 */
void process_chatmode(const char* host, const int port)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In order to join the chatroom, you are supposed to connect
	// to the server using host and port.
	// You may re-use the function "connect_to".
	// ------------------------------------------------------------
	int sockfd, *sockptr;
	
	// ------------------------------------------------------------
	// GUIDE 2:
	// Once the client have been connected to the server, we need
	// to get a message from the user and send it to server.
	// At the same time, the client should wait for a message from
	// the server.
	// ------------------------------------------------------------
	
	sockfd = connect_to(host, port);
	
	sockptr = malloc(1);	
    *sockptr = sockfd;
	
	// Helper thread listens on port for other users' chat messages
	pthread_t helper;
	printf("Creating thread w socket %i\n", sockfd);
	pthread_create(&helper, NULL, client_helper, (void*) sockptr);
	
	struct Server* serv = malloc(sizeof(struct Server));
	serv->host = (char*)malloc(129);
	strcpy(serv->host, host);
	serv->port = port;
	
	// Parent thread listens for user input
	while(1) {
		if(sockfd < 0) {
			perror("Connection failed");
			exit(EXIT_FAILURE);
		}
		
		char message[MAX_DATA];
	  	get_message(message, MAX_DATA);
	  	
	  	// printf("sending chat message: %s\n", message);
		
		if(send(sockfd, message, strlen(message), 0) < 0) {
			perror("Send failed");
			continue;
		}
	}
  // ------------------------------------------------------------
  // IMPORTANT NOTICE:
  // 1. To get a message from a user, you should use a function
  // "void get_message(char*, int);" in the interface.h file
  //
  // 2. To print the messages from other members, you should use
  // the function "void display_message(char*)" in the interface.h
  //
  // 3. Once a user entered to one of chatrooms, there is no way
  //    to command mode where the user  enter other commands
  //    such as CREATE,DELETE,LIST.
  //    Don't have to worry about this situation, and you can
  //    terminate the client program by pressing CTRL-C (SIGINT)
	// ------------------------------------------------------------
}

/*
 * Listens for chat room messages
 *
 * @parameter in_socket file descriptor of 
 *
 */
void* client_helper(void* in_socket) 
{
	//  // Socket values
	// int c;
	// sockfd = *(int*)in_socket
	// int opt = 1;
	
	// // initialize socket
	// if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	// 	perror("socket failed\n");
	// 	exit(EXIT_FAILURE);
	// }
	// printf("socket done\n");
	
	// if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	// {
	// 	perror("setsockopt");
	// 	exit(EXIT_FAILURE);
	// }
	
	// if((bind(sockfd, (struct sockaddr *) &server, sizeof(server))) < 0) {
	// 	perror("bind failed\n");
	// 	exit(EXIT_FAILURE);
	// }
	// printf("bind done\n");
	
	// if(listen(sockfd, 10) < 0) {
	// 	perror("listen\n");
	// 	exit(EXIT_FAILURE);
	// }
	// printf("listening...\n");
	
	// // Create threads for each connection
	// c = sizeof(struct sockaddr_in);
	// while((new_socket = accept(sockfd, (struct sockaddr *)&server, (socklen_t*)&c))) {
	  
	// 	pthread_t sniffer_thread;
	// 	new_sock = malloc(1);
	// 	*new_sock = new_socket;
	
	// 	if( pthread_create( &sniffer_thread, NULL, connection_handler, (void*) new_sock) < 0) {
	// 		perror("could not create thread");
	// 		return 1;
	// 	}
	// }
	// struct sockaddr_in server;
	
	// server.sin_family = AF_INET;
	// server.sin_port = htons( port );
	// server.sin_addr.s_addr = inet_addr( host );
	
	// if (connect(*(int*)in_socket, (struct sockaddr *)&server, sizeof(server)) < 0)
	// {
	// 	perror("connect error\n");
	// 	return 0;
	// }
	
	int recval, sockfd;
	char message[MAX_DATA];
	sockfd = *(int*)in_socket;
	
	printf("Client listening on socket %i\n", sockfd);
	
	while(recval = recv(sockfd, message, MAX_DATA, 0)) {
		message[strlen(message)] = '\0';
		if(recval == 0) {
			perror("recv failed");
			continue;
		}
		if(recval < 0) {
			perror("recv failed");
			continue;
		}
		if(strlen(message) == 0) {
			continue;
		}
		// printf("Received: %.*s\n", strlen(message), message);
		display_message(message);
		printf("\n");
	}
	return 0;
}