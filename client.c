#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "SBCP.h"
#include <inttypes.h>

int root;
int gotack = 0, blink = 1;
pthread_t  thread_id;
pthread_mutex_t lock;
clock_t tstart;	

void encode(struct SBCPM message, char *ptr){
	uint32_t host,network;
	
	memcpy((char *)&host, (char *)(&message.header), 4);
	//printf("%" PRIu16 "\n", (uint16_t)host);
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


void decode(struct SBCPM *message, char abuffer[]){
	uint32_t host,network;
	char *ptr = &abuffer[0];

	memcpy((char *)&network, (char *)ptr, 4);
	host = ntohl(network);
	message->header.vrsn = host & 0x000001ff;
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
	printf("%s\n", message->attribute[1].payload);
}

int patchback(int branch){
	char abuffer[2048];
	struct SBCPM message;
	memset(&message, 0, sizeof(message));
	char *one = "1";
	char *two = "2";
	if(read(branch,abuffer,2048)<=0){
		printf("Disconnected!\n");
		return 1;
	}
	else{
		decode(&message,abuffer);
		switch(message.header.type){
			case ACK:
			printf("        [Members online(%s): %s]\n\nYou: ",message.attribute[0].payload, message.attribute[1].payload);
			tstart = clock();
			gotack = 1;
			break;
			case NAK:
			if(strcmp(message.attribute[0].payload,one)==0) printf("[Err: Username exists!]\n");
			else if(strcmp(message.attribute[0].payload,two)==0) printf("[Err: User limit reached!]\n");
			break;
			case ONLINE:
			printf("\n    [ONLINE: %s]\nYou: ",message.attribute[0].payload);
			break;
			case OFFLINE:
			printf("\n    [OFFLINE: %s]\nYou: ",message.attribute[0].payload);
			break;
			case FWD:
			printf("\n%s: %s\nYou: ",message.attribute[0].payload, message.attribute[1].payload);
			break;
			case IDLE:
			printf("\n    [IDLE: %s]\nYou: ",message.attribute[0].payload);
			break;
			default:
			break;
		}
		fflush(stdout);
		return 0;
	}
}

void dispatch(int branch, enum m_type type, char const *tag){
	struct SBCPM message;
	struct SBCPH header;
	struct SBCPA attribute[2];
	memset(&message, 0, sizeof(message));
	memset(&header, 0, sizeof(header));
	memset(&attribute, 0, sizeof(attribute));
	char abuffer[2048];
	header.vrsn = 3;

	switch (type) {
		case JOIN:
		header.type = JOIN;
		attribute[0].type = USERNAME;
		strcpy(attribute[0].payload, tag);
		break;
		case SEND:
		header.type = SEND;
		attribute[0].type = MESSAGE;
		strcpy(attribute[0].payload, tag);
		break;
		case IDLE:
		header.type = IDLE;
		break;
		default:
		break;
	}
	attribute[0].length = strlen(attribute[0].payload);
	attribute[1].length = 0;
	message.header = header;
	message.header.length = sizeof(attribute)+4;
	message.attribute[0] = attribute[0];
	message.attribute[1] = attribute[1];
	encode(message,&abuffer[0]);
	write(branch,abuffer,message.header.length);
	// if(patchback(branch) == 1) close(branch);
}

void nexus(char const *target[]){
	int joined = 0;
	struct sockaddr_in rootaddr;
	struct hostent* myhost;
	fd_set tree, reads;
	FD_ZERO(&reads);
	FD_ZERO(&tree);
	char message[512];

	if((root = socket(AF_INET,SOCK_STREAM,0))==-1) {
		printf("Connection failed\n");
		exit(EXIT_FAILURE);
	}
	bzero(&rootaddr,sizeof(rootaddr));
	rootaddr.sin_family=AF_INET;
	myhost = gethostbyname(target[2]);
	memcpy(&rootaddr.sin_addr.s_addr, myhost->h_addr,myhost->h_length);
	rootaddr.sin_port = htons(atoi(target[3]));

	if(connect(root,(struct sockaddr *)&rootaddr,sizeof(rootaddr))==-1){
		printf("Connection failed\n");
		exit(EXIT_FAILURE);
	}
	else{
		printf("    [Welcome! Enter JOIN to participate]: ");
		fflush(stdout);

		FD_SET(root, &tree);
		FD_SET(STDIN_FILENO, &tree);
		for(;;){
			reads = tree;
			if(select(root+1, &reads,NULL,NULL,NULL)==-1){
				perror("select");
				exit(4);
			}
			if(FD_ISSET(root,&reads)) if(patchback(root)==1) exit(EXIT_FAILURE);
			if(FD_ISSET(STDIN_FILENO,&reads)){
				int copper;
				bzero(message,512);
				if((copper = read(STDIN_FILENO, message,sizeof(message)))>0){
					char *key = "JOIN\n";
					if(joined==0 && strcmp(message,key)==0){
						dispatch(root,JOIN,target[1]);
						joined = 1;
					}
					else if(joined==1) {
						dispatch(root,SEND,message);
						fflush(stdout);
						tstart = clock();
						blink = 1;
					}
					else printf("Please JOIN the session first.\n");
				}
			}
		}
	}
}

void *checkIDLE(void *tag){
	while(1){
		pthread_mutex_lock(&lock);
		if(blink == 1){
			if(gotack && ((clock()-tstart)> IDLETIME)) {
				dispatch(root,IDLE,0);
				blink=0;
			}
		}
		pthread_mutex_unlock(&lock);
	}
}

int main(int argc, char const *argv[]){
	pthread_mutex_init(&lock, NULL);
	pthread_create(&thread_id, NULL, checkIDLE, (void*)NULL);
	if(argc==4) nexus(argv);
	else printf("Format: %s <username> <server> <port>\n", argv[0]);
	pthread_mutex_destroy(&lock);
	pthread_join(thread_id, NULL);
	return 0;
}