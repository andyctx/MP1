#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/wait.h>
#include <semaphore.h>

#include "interface.h"

#define MAX_MEMBER 256
#define MAX_ROOM 256

struct Room {
    char room_name[256];
    int port;
    int num_member;
    int* slave_socket;
    int active;
};

struct helper_args {
    int sock;
    struct Room* room;
};

// Chatroom information
struct Room* room_db[MAX_ROOM] = {NULL};
int curr_port = 0;
int num_rooms = 0;
sem_t rm_mutex;
sem_t ctr_mutex;


void* connection_handler(void *sock_desc);

struct Reply process_command(int sockfd, char* command, char* name);

void* chatroom(void* name);

void* cr_slave(void *sock_desc);

int main(int argc, char** argv) {
    if (argc != 2) {
    	fprintf(stderr, "usage: enter port number\n");
    	exit(1);
    }
    
    // printf("ROOMDB: %p\n", (void *) &room_db);
    
    sem_init(&rm_mutex, 0, 1);
    sem_init(&ctr_mutex, 0, 1);
    
    // Socket values
    char *p;
    int port = strtol(argv[1], &p, 10);
    int sockfd, c, new_socket, *new_sock;
    struct sockaddr_in server;
    int opt = 1;
    
    sem_wait(&ctr_mutex);
    curr_port += port + 1;
    sem_post(&ctr_mutex);
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( port );
    
    // initialize socket
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed\n");
        exit(EXIT_FAILURE);
    }
    printf("socket done\n");

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if((bind(sockfd, (struct sockaddr *) &server, sizeof(server))) < 0) {
        perror("bind failed\n");
        exit(EXIT_FAILURE);
    }
    printf("bind done\n");

    if(listen(sockfd, 10) < 0) {
        perror("listen\n");
        exit(EXIT_FAILURE);
    }
    printf("listening...\n");
  
    // Create threads for each connection
    c = sizeof(struct sockaddr_in);
    while((new_socket = accept(sockfd, (struct sockaddr *)&server, (socklen_t*)&c))) {
        
        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = new_socket;
    
        if( pthread_create( &sniffer_thread, NULL, connection_handler, (void*) new_sock) < 0) {
    		perror("could not create thread");
    		return 1;
    	}
    }
    
    sem_wait(&rm_mutex);
    for(int i = 0; i < sizeof(room_db); i++) {
        free(room_db[i]);
    }
    sem_post(&rm_mutex);
    
    return 0;
}

void* connection_handler(void *sock_desc) {
    int valread;
    char buffer[2000];
    struct Reply* message[2000];
    struct Reply reply;
    int sockfd = *(int*) sock_desc;
    
    while( (valread = read(sockfd, buffer, 2000)) > 0 ) {
        char* command = (char*) malloc(128);
        char* name = (char*) malloc(128);
        char* token = strtok(buffer, " ");
        strcpy(command, token);
        token = strtok(NULL, " ");
        if(token != NULL) {
            strcpy(name, token);
        } else {
            name = NULL;
        }
        
        reply = process_command(sockfd, command, name);
        memcpy(message, &reply, sizeof(reply));
        write(sockfd, message, sizeof(message));
        
        // if(strcmp(command, "JOIN") == 0) {
        //     sleep(1000000);
        // }
        
        free(command);
    }
     // Client disconnection
    if(valread == 0) {
        printf("Client disconnected\n");
        fflush(stdout);
    }
    
    // Read failed
    if(valread == -1) {
        int e = errno;
        printf("read failed: %i\n", e);
    }
}

struct Reply process_command(int sockfd, char* command, char* name) {
    printf("in process_command: %s %s\n", command, name);
    touppercase(command, strlen(command) - 1);
    struct Reply reply;
    
    if(strcmp(command, "CREATE") == 0 && name != NULL) {
        // Check if room exists
        sem_wait(&rm_mutex);
        int room_idx = -1;
        for(int i = 0; i < sizeof(MAX_ROOM); i++) {
            if(room_db[i] != NULL) {
                struct Room* room = (struct Room*)(room_db[i]);
                printf("Room %s, %i\n", room->room_name, room->port);
                if(strcmp(room->room_name, name) == 0) {
                    printf("Room exists\n");
                    room_idx = i;
                    break;
                }
            }
        }
        sem_post(&rm_mutex);
        
        // Room exists
        if(room_idx != -1) {
            reply.status = FAILURE_ALREADY_EXISTS;
            return reply;
        }
        
        // Create room thread
        printf("Creating %s\n", name);
        pthread_t cr_thread;
        if( pthread_create( &cr_thread, NULL, chatroom,(void*) name) < 0) {
    		perror("Could not create thread");
    		reply.status = FAILURE_INVALID;
            return reply;
    	}
        reply.status = SUCCESS;
        return reply;
    } else if(strcmp(command, "JOIN") == 0 && name != NULL) {
        printf("Joining %s\n", name);
        
        // Search for room
        sem_wait(&rm_mutex);
        int room_idx = -1;
        for(int i = 0; i < sizeof(MAX_ROOM); i++) {
            if(room_db[i] != NULL) {
                struct Room* room = (struct Room*)(room_db[i]);
                printf("%s, %i\n", room->room_name, room->port);
                if(strcmp(room->room_name, name) == 0) {
                    // printf("Room exists\n");
                    room_idx = i;
                    break;
                }
            }
        }
        sem_post(&rm_mutex);
        
        // printf("%i\n", room_idx);
        if(room_idx == -1) {
            reply.status = FAILURE_NOT_EXISTS;
            return reply;
        }
        
        sem_wait(&rm_mutex);
        room_db[room_idx]->num_member++;
        reply.num_member = room_db[room_idx]->num_member;
        reply.port = room_db[room_idx]->port;
        
        room_db[room_idx]->num_member++;
        sem_post(&rm_mutex);

        reply.status = SUCCESS;
        return reply;
    } else if(strcmp(command, "DELETE") == 0 && name != NULL) {
        printf("Deleting %s\n", name);
        
        // Search for room
        sem_wait(&rm_mutex);
        int room_idx = -1;
        for(int i = 0; i < sizeof(MAX_ROOM); i++) {
            if(room_db[i] != NULL) {
                struct Room* room = (struct Room*)(room_db[i]);
                printf("%s, %i\n", room->room_name, room->port);
                if(strcmp(room->room_name, name) == 0) {
                    // printf("Room exists\n");
                    room_idx = i;
                    break;
                }
            }
        }
        sem_post(&rm_mutex);
        
        if(room_idx == -1) {
            reply.status = FAILURE_NOT_EXISTS;
            return reply;
        }
        
        for(int i = 0; i < MAX_MEMBER; i++) {
            char* message = "Warning: the chat room is going to be closed...";
            int slave_socket = room_db[room_idx]->slave_socket[i];
            if(slave_socket != -1) {
                printf("Sending to %i\n", room_db[room_idx]->slave_socket[i]);
                write(slave_socket, message, sizeof(message));
            }
        }
        
        return reply;
    } else if (strcmp(command, "LIST") == 0) {
        printf("Listing\n");
        
        int found = 0;
        char outbuf[MAX_DATA];
        
        // Search for room
        sem_wait(&rm_mutex);
        for(int i = 0; i < sizeof(MAX_ROOM); i++) {
            if(room_db[i] != NULL) {
                found = 1;
                struct Room* room = (struct Room*)(room_db[i]);
                strcat(outbuf, room->room_name);
                strcat(outbuf, ", ");
            }
        }
        sem_post(&rm_mutex);
        
        if(found) {
            reply.status = SUCCESS;
            memcpy(reply.list_room, outbuf, strlen(outbuf) - 2);
            printf("Found");
        } else {
            reply.status = SUCCESS;
            strcpy(reply.list_room, "No chat rooms");
            printf("Not found");
        }
        
        
        return reply;
    }
    reply.status = FAILURE_INVALID;
    return reply;
}

void* chatroom(void* name) {
    
    // Create room
    struct Room *roomptr = malloc (sizeof (struct Room));
    roomptr->slave_socket = malloc(MAX_DATA * sizeof(int) + 1);
    
    // Fill slave sockets
    for(int i = 0; i < MAX_MEMBER; i++) {
        roomptr->slave_socket[i] = -1;
    }
    
    sem_wait(&ctr_mutex);
    roomptr->port = curr_port++;
    sem_post(&ctr_mutex);
    
    roomptr->num_member = 0;
    roomptr->active = 1;
    strcpy(roomptr->room_name, (char*)name);
    
    num_rooms++;
    struct Room room = *roomptr;
    
    // Add room to db
    sem_wait(&rm_mutex);
    for(int i = 0; i < MAX_ROOM; i++) {
        if(room_db[i] == NULL) {
            printf("Room stored in db\n");
            room_db[i] = roomptr;
            printf("%s\n", room_db[i]->room_name);
            break;
        }
    }
    sem_post(&rm_mutex);
    
    int sockfd, c, new_socket, *new_sock;
    struct sockaddr_in server;
    int opt = 1;
    c = sizeof(struct sockaddr_in);
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( room.port );
    
    // Create socket
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed\n");
        exit(EXIT_FAILURE);
    }
    printf("socket done\n");

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if((bind(sockfd, (struct sockaddr *) &server, sizeof(server))) < 0) {
        perror("bind failed\n");
        exit(EXIT_FAILURE);
    }
    printf("bind done\n");

    if(listen(sockfd, 10) < 0) {
        perror("listen\n");
        exit(EXIT_FAILURE);
    }
    printf("chatroom %s with port: %i listening...\n", roomptr->room_name, roomptr->port);
    
    // Listen for new connections
    while((new_socket = accept(sockfd, (struct sockaddr *)&server, (socklen_t*)&c))) {
        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = new_socket;
        
        // Debugging print
        printf("New socket: %i\n", new_socket);        
        
        // Search if room exists
        sem_wait(&rm_mutex);
        int room_idx = -1;
        
        // printf("Comparing %s\n", (char*)name);
        for(int i = 0; i < MAX_ROOM; i++) {
            if(room_db[i] != NULL) {
                struct Room* curr_room = (struct Room*)(room_db[i]);
                if(strcmp(curr_room->room_name, (char*)name) == 0) {
                    printf("Room exists at %i\n", i);
                    room_idx = i;
                    printf("Number: %i\n", room_idx);
                    break;
                }
            }
        }
        printf("\n");
        
        // printf("Number: %i\n", room_idx);
        
        // Add socket fd to slave sockets
        if((room_db[room_idx]) != NULL) {
            for(int i = 0; i < MAX_ROOM; i++) {
                int slave_socket = room_db[room_idx]->slave_socket[i];
                printf("%i ", slave_socket);
                if(slave_socket == -1) {
                    printf("ss added\n");
                    room_db[room_idx]->slave_socket[i] = new_socket;
                    break;
                }
            }
        }
        sem_post(&rm_mutex);
        
        struct helper_args arguments;
        arguments.sock = new_socket;
        arguments.room = roomptr;
        struct helper_args* args = (struct helper_args*)malloc(1);
        *args = arguments;
        
        if( pthread_create( &sniffer_thread, NULL, cr_slave, (void*) args) < 0) {
    		perror("could not create thread");
    		return 0;
    	}
    }
    free(name);
    return 0;
}

void* cr_slave(void* args) {
    struct helper_args* arguments = (struct helper_args*) args;
    struct Room* room = arguments->room;
    int sockfd = arguments->sock;
    
    int valread;
    
    sem_wait(&rm_mutex);
    
    int room_idx = -1;
    for(int i = 0; i < MAX_ROOM; i++) {
        if(room_db[i] != NULL) {
            struct Room* curr_room = (struct Room*)(room_db[i]);
            if(strcmp(curr_room->room_name, room->room_name) == 0) {
                room_idx = i;
                break;
            }
        }
    }
    
    char message[MAX_DATA];
    while( (valread = read(sockfd, message, MAX_DATA)) > 0 ) {
        if (valread >= 0)
            message[valread] = '\0';
        if(strcmp(message, "list") == 0) {
            sem_wait(&rm_mutex);
            printf("Slave sockets cr_slave:\n");
            for(int i = 0; i < MAX_MEMBER; i++) {
                int slave_socket = room_db[room_idx]->slave_socket[i];
                printf("%i ", room_db[room_idx]->slave_socket[i]);
            }
            printf("\n");
            sem_post(&rm_mutex);
        }
    
        printf("Socket %i: %s\n", sockfd, message);
        
        for(int i = 0; i < MAX_MEMBER; i++) {
            int slave_socket = room_db[room_idx]->slave_socket[i];
            if(slave_socket != -1 && slave_socket != sockfd) {
                printf("Sending to %i\n", room_db[room_idx]->slave_socket[i]);
                write(slave_socket, message, sizeof(message));
            }
        }
    }
    
    // Client disconnection
    if(valread == 0) {
        printf("Client on socket %i disconnected\n", sockfd);
        fflush(stdout);
        close(sockfd);
        for(int i = 0; i < MAX_MEMBER; i++) {
            int slave_socket = room_db[room_idx]->slave_socket[i];
            if(slave_socket == sockfd) {
                room_db[room_idx]->slave_socket[i] = -1;
            }
        }
        return 0;
    }
    
    // Read failed
    if(valread == -1) {
        printf("read failed on socket %i\n", sockfd);
        close(sockfd);
        for(int i = 0; i < MAX_MEMBER; i++) {
            int slave_socket = room_db[room_idx]->slave_socket[i];
            if(slave_socket == sockfd) {
                room_db[room_idx]->slave_socket[i] = -1;
            }
        }
        return 0;
    }
}