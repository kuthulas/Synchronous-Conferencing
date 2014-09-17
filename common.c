#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include "SBCP.h"

void codec(struct SBCPM message, char abuffer[1024], char *bbuffer, int type){
	uint16_t host,network;
	if(type==0){ // HosttoNetwork
		char *ptr = &abuffer[0];
		memcpy((char *)&host, (char *)(&message.header), 2);
		network = htons(host);
		memcpy(ptr, (const char *)&network,2);
		ptr += 2;
		memcpy((char *)&host, (char *)(&message.header)+2, 2);
		network = htons(host);
		memcpy(ptr, (const char *)&network,2);
		ptr += 2;
		memcpy((char *)&host, (char *)(&message.attribute[0]), 2);
		network = htons(host);
		memcpy(ptr, (const char *)&network,2);
		ptr += 2;
		memcpy((char *)&host, (char *)(&message.attribute[0])+2, 2);
		network = htons(host);
		memcpy(ptr, (const char *)&network,2);
		ptr += 2;
		memcpy(ptr, (char *)(&message.attribute[0].payload), strlen(message.attribute[0].payload));
		ptr += strlen(message.attribute[0].payload);
		memcpy((char *)&host, (char *)(&message.attribute[1]), 2);
		network = htons(host);
		memcpy(ptr, (const char *)&network,2);
		ptr += 2;
		memcpy((char *)&host, (char *)(&message.attribute[1])+2, 2);
		network = htons(host);
		memcpy(ptr, (const char *)&network,2);
		ptr += 2;
		memcpy(ptr, (char *)(&message.attribute[1].payload), strlen(message.attribute[1].payload));
		ptr += strlen(message.attribute[1].payload);
	}
	else{ // NetworktoHost
		char *ptr = &bbuffer[0];
		uint16_t vrsn, htype, hlength, atype, alength;

		memcpy((char *)&network, (char *)ptr, 2);
		host = ntohs(network);
		vrsn = host & 0x003f;
		ptr += 2;

		memcpy((char *)&network, (char *)ptr, 2);
		host = ntohs(network);
		host = host & 0xff00;
		htype = host >> 9;
		ptr += 2;

		memcpy((char *)&network, (char *)ptr, 2);
		host = ntohs(network);
		hlength = host;
		ptr += 2;

		memcpy((char *)&network, (char *)ptr, 2);
		host = ntohs(network);
		atype = host;
		ptr += 2;

		memcpy((char *)&network, (char *)ptr, 2);
		host = ntohs(network);
		alength = host;
		ptr += 2;

		int len = alength - 4;
		memcpy(abuffer, ptr, len);
		abuffer[len+1] = '\0';
	}
}
