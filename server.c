/**
 * Authors:	Kumaran Thulasiraman, Jnanesh Manjunath
 * Created:	Sep 23, 2014
 *
 * ECEN 602: Computer Communications and Networking
 * Texas A&M University
 **/

#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "SBCP.h"

 int root, nclients=0, mclients, fdmax;
 char buffer[512];
 fd_set tree;
 struct client *clients; // Array to hold connected-client details

// Message struct to byte-stream conversion and byte ordering (Integer; 4 byte sets)
 void encode(struct SBCPM message, char *ptr){
 	uint32_t host,network;

 	memcpy((char *)&host, (char *)(&message.header), 4);
 	network = htonl(host);
 	memcpy(ptr, (const char *)&network,4);
 	ptr += 4;

 	memcpy((char *)&host, (char *)(&message.attribute[0]), 4);
 	network = htonl(host);
 	memcpy(ptr, (const char *)&network,4);
 	ptr += 4;

 	memcpy(ptr, (char *)(&message.attribute[0].payload), strlen(message.attribute[0].payload));
 	ptr += strlen(message.attribute[0].payload);

 	memcpy((char *)&host, (char *)(&message.attribute[1]), 4);
 	network = htonl(host);
 	memcpy(ptr, (const char *)&network,4);
 	ptr += 4;

 	memcpy(ptr, (char *)(&message.attribute[1].payload), strlen(message.attribute[1].payload));
 	ptr += strlen(message.attribute[1].payload);
 }

// Byte-stream conversion to message struct and byte ordering
 void decode(struct SBCPM *message, char abuffer[]){
 	uint32_t host,network;
 	char *ptr = &abuffer[0];

 	memcpy((char *)&network, (char *)ptr, 4);
 	host = ntohl(network);
	message->header.vrsn = host & 0x000001ff; // Bit masking to extract bit-fields
	message->header.type = (host & 0x0000fe00) >> 9;
	message->header.length = (host & 0xffff0000) >> 16;
	ptr += 4;
	
	memcpy((char *)&network, (char *)ptr, 4);
	host = ntohl(network);
	message->attribute[0].type = host & 0x0000ffff;
	message->attribute[0].length = (host & 0xffff0000) >> 16;
	ptr += 4;

	memcpy(message->attribute[0].payload, ptr, message->attribute[0].length);
	ptr += message->attribute[0].length;
	
	memcpy((char *)&network, (char *)ptr, 4);
	host = ntohl(network);
	message->attribute[1].type = host & 0x0000ffff;
	message->attribute[1].length = (host & 0xffff0000) >> 16;
	ptr += 4;

	memcpy(message->attribute[1].payload, ptr, message->attribute[1].length);
	ptr += message->attribute[1].length;
}

// Creates host socket
void nexus(char const *target[]){

	struct addrinfo hints, *servinfo, *p;
	int rv, yes=1;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(target[1], target[2], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(EXIT_FAILURE);
	}

	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((root = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		if (setsockopt(root, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(EXIT_FAILURE);
		}
		if (bind(root, p->ai_addr, p->ai_addrlen) == -1) {
			close(root);
			perror("server: bind");
			continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(servinfo);
	if (listen(root, atoi(target[3])) == -1) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	FD_SET(root, &tree);
}

// Check for duplicate in username
int userexists(char user[]){
	int exists = 0;
	int u;
	for(u = 0; u < nclients ; u++){
		if(!strcmp(user,clients[u].name)){
			exists = 1;
			break;
		}
	}
	return exists;
}

// Cleanup: Remove descriptor from the FD set, and from the client struct array
void cleanup(int branch){
	FD_CLR(branch,&tree);
	int i,j;
	for(j=0;j<nclients;j++){
		if(clients[j].fd==branch){
			for(i=j;i<nclients;i++) clients[i]=clients[i+1];
				nclients--;
			break;
		}
	}
}

// UNICAST and MULTICAST messaging to connected clients
void msgplex(int branch, struct SBCPM message, enum c_type code){
	char abuffer[2048];
	encode(message,&abuffer[0]);

	if(code==UNICAST) write(branch,abuffer,strlen(message.attribute[0].payload)+strlen(message.attribute[1].payload)+12);
	else{
		int c,i;
		for(c = 0; c <= fdmax; c++) {
			if (FD_ISSET(c, &tree)) {
				if (c != root && c != branch){
					for(i=0;i<nclients;i++) if(clients[i].fd == c) if ((write(c,abuffer,strlen(message.attribute[0].payload)+strlen(message.attribute[1].payload)+12)) == -1){
						printf("msgplex failed\n");
					}
				}
			}       
		}
	}
}

// Message packing and dispatching - Handles ACK, NAK, ONLINE, OFFLINE, FWD and IDLE for the server
void dispatch(int branch, enum m_type type, int tag, enum c_type code){
	struct SBCPM message;
	struct SBCPH header;
	struct SBCPA attribute[2];
	header.vrsn = 3;	
	int i;

	switch (type) {
		case ACK:
		header.type = ACK;
		attribute[0].type = MESSAGE;
		attribute[1].type = COUNT;
		sprintf(attribute[0].payload,"%d", nclients);
		if(nclients==1) strcpy(attribute[1].payload, "Just you!");
		else {
			int c;
			strcpy(attribute[1].payload,"You,");
			for(c=0; c<nclients-1; c++){
				strcat(attribute[1].payload,clients[c].name);
				if(c!=nclients-2) strcat(attribute[1].payload, ",");
				else strcat(attribute[1].payload, ".");
			}
		}
		break;
		case NAK:
		header.type = NAK;
		attribute[0].type = REASON;
		sprintf(attribute[0].payload, "%d", tag);
		break;
		case ONLINE:
		header.type = ONLINE;
		attribute[0].type = USERNAME;
		strcpy(attribute[0].payload,clients[nclients-1].name);
		break;
		case OFFLINE:
		header.type = OFFLINE;
		attribute[0].type = USERNAME;
		strcpy(attribute[0].payload,clients[tag].name);
		printf("[Client disconnected]\n");
		break;
		case IDLE:
		header.type = IDLE;
		attribute[0].type = USERNAME;
		strcpy(attribute[0].payload,clients[tag].name);
		break;
		case FWD:
		header.type = FWD;
		attribute[0].type = MESSAGE;
		attribute[1].type = USERNAME;
		strcpy(attribute[1].payload,buffer);
		for(i=0;i<nclients;i++) if(clients[i].fd==branch) strcpy(attribute[0].payload,clients[i].name);
			break;
		default:
		break;
	}

	attribute[0].length = strlen(attribute[0].payload);
	attribute[1].length = strlen(attribute[1].payload);
	message.header = header;
	message.header.length = sizeof attribute;
	message.attribute[0] = attribute[0];
	message.attribute[1] = attribute[1];

	msgplex(branch, message, code);

	if(type==NAK){
		close(branch);
		cleanup(branch);
	}
}

// Manages handshaking with the connected client - Decides on response for JOIN - NAK, ACK & ONLINE
void handshake(int branch, struct SBCPM message){
	struct SBCPA attribute = message.attribute[0];;
	char user[16];
	strcpy(user, attribute.payload);
	int exists = userexists(user);
	if(exists==1) dispatch(branch,NAK,1,UNICAST);
	else if(nclients == mclients) dispatch(branch,NAK,2,UNICAST);
	else{
		strcpy(clients[nclients].name, user);
		clients[nclients].fd = branch;
		nclients++;
		dispatch(branch,ACK,0,UNICAST);
		dispatch(branch,ONLINE,0,MULTICAST);
		printf("[New client accepted]\n");
	}
}

// Main; Handles I/O multiplexing. Session handling logic is here.
int main(int argc, char const *argv[]){
	if(argc==4){
		mclients = atoi(argv[3]);
		int branch, gold, c;
		fd_set reads;

		struct sockaddr_storage address;
		struct SBCPM message;

		nexus(argv);
		printf("[Chat server ready!]\n");
		fdmax = root;
		clients= (struct client *)malloc(mclients*sizeof(struct client));

		for(;;){
			reads = tree;
			if(select(fdmax+1, &reads, NULL,NULL,NULL)==-1){
				perror("select");
				exit(EXIT_FAILURE);
			}

			for(c=0;c<=fdmax;c++){
				char abuffer[2048];
				memset(&message, 0, sizeof(message));
				if(FD_ISSET(c, &reads)){
					if(c==root){ // New client request
						socklen_t len = sizeof address;
						if((branch = accept(root,(struct sockaddr *)&address,&len))!=-1){
							FD_SET(branch, &tree);
							if(branch > fdmax) fdmax = branch;
						}
					}
					else{
						if((gold=read(c,abuffer,2048))>0){ // Pending messages from a client
							decode(&message,abuffer);
							if(message.header.type == JOIN) handshake(c,message); // Initiate handshake upon a JOIN request
							else if(message.header.type == SEND) { // Forward messages if message type is FWD
								strcpy(buffer, message.attribute[0].payload);
								buffer[strlen(buffer) - 1] = '\0';
								dispatch(c,FWD,0,MULTICAST);
							}
							else if(message.header.type == IDLE){ // Multicast IDLE messages from a client to all other clients
								int j;
								for(j=0;j<nclients;j++){
									if(clients[j].fd==c) dispatch(c,IDLE,j,MULTICAST);
								}
							}
						}
						else{ // Connection error handling & cleanup
							if(gold==0){ // Connection gracefully closed by client; initiate OFFLINE multicasting
								int i;
								for(i=0;i<nclients;i++) if(clients[i].fd==c){
									dispatch(0,OFFLINE,i,MULTICAST);
								}
							}
							else perror("recv"); // Connection error
							close(c);
							cleanup(c);
						}
					}
				}
			}
		}
	}
	else printf("Format: %s <server> <port> <maxClients>\n", argv[0]);
	return 0;
}
