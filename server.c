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
struct client *clients;

void nexus(char const *target[]){
	struct sockaddr_in rootaddr;
	struct hostent* myhost;
	if ((root = socket(AF_INET,SOCK_STREAM,0)) == -1) exit(EXIT_FAILURE);
	bzero(&rootaddr,sizeof(rootaddr));
	rootaddr.sin_family = AF_INET;
	myhost = gethostbyname(target[1]);
	memcpy(&rootaddr.sin_addr.s_addr, myhost->h_addr,myhost->h_length);
	rootaddr.sin_port = htons(atoi(target[2]));
	if((bind(root, (struct sockaddr*)&rootaddr, sizeof(rootaddr))) == -1) exit(EXIT_FAILURE);
	if((listen(root, atoi(target[3])))==-1) exit(EXIT_FAILURE);
	FD_SET(root, &tree);
}

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

void msgplex(int branch, struct SBCPM message, enum c_type code){
	if(code==UNICAST) write(branch,(void *) &message,sizeof(message));
	else{
		int c;
		for(c = 0; c <= fdmax; c++) {
			if (FD_ISSET(c, &tree)) {
				if (c != root && c != branch){
					if ((write(c,(void *) &message,sizeof(message))) == -1){
						printf("msgplex failed\n");
					}
				}
			}       
		}
	}
}

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
		sprintf(attribute[1].payload,"%d", nclients);
		if(nclients==1) strcpy(attribute[0].payload, "Just you!");
		else {
			int c;
			strcpy(attribute[0].payload,"You,");
			for(c=0; c<nclients-1; c++){
				strcat(attribute[0].payload,clients[c].name);
				if(c!=nclients-2) strcat(attribute[0].payload, ",");
				else strcat(attribute[0].payload, ".");
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
		strcpy(attribute[0].payload,buffer);
		for(i=0;i<nclients;i++) if(clients[i].fd==branch) strcpy(attribute[1].payload,clients[i].name);
			break;
		default:
		break;
	}

	attribute[0].length = strlen(attribute[0].payload);
	attribute[1].length = strlen(attribute[1].payload);
	message.header = header;
	message.attribute[0] = attribute[0];
	message.attribute[1] = attribute[1];

	msgplex(branch, message, code);

	if(type==OFFLINE || type==NAK){
		close(branch);
		cleanup(branch);
	}
}

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
				if(FD_ISSET(c, &reads)){
					if(c==root){
						socklen_t len = sizeof address;
						if((branch = accept(root,(struct sockaddr *)&address,&len))!=-1){
							FD_SET(branch, &tree);
							if(branch > fdmax) fdmax = branch;
						}
					}
					else{
						if((gold=read(c,(struct SBCPM *) &message,sizeof(message)))>0){
							if(message.header.type == JOIN) handshake(c,message);
							else if(message.header.type == SEND) {
								strcpy(buffer, message.attribute[0].payload);
								buffer[strlen(buffer) - 1] = '\0';
								dispatch(c,FWD,0,MULTICAST);
							}
							else if(message.header.type == IDLE){
								int j;
								for(j=0;j<nclients;j++){
									if(clients[j].fd==c) dispatch(c,IDLE,j,MULTICAST);
								}
							}
						}
						else{
							if(gold==0){
								int i;
								for(i=0;i<nclients;i++) if(clients[i].fd==c)dispatch(0,OFFLINE,i,MULTICAST);
							}
						else perror("recv");
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