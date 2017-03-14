#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>

#define DEBUG
#define MAXDATALEN 1000
#define lenname 10 //MAX_NAME
#define lendata 1024

struct packet{
	char type[10];
	unsigned int size;
	char uid[lenname];
	char data[lendata];
};


//read contents from buffer into struct packet
void read_buffer(char buffer[], struct packet* _packet){
	unsigned int fake_len = strlen(buffer);
	int i=0, j=0, offset=0, count=0;
	for(i = 0; i<fake_len; ++i){
		if(buffer[i] != ':'){
			++j;
		}else{
			if(count==0)
				memcpy((void*)_packet->type,buffer+offset,j);
			else if(count==1)
				memcpy(&(_packet->size),buffer+offset,j);
			else if(count==2){
				memcpy((void*)_packet->uid,buffer+offset,j);
				offset=offset+j+1;
				memcpy((void*)_packet->data,buffer+offset,_packet->size);
				return;
			}
			offset=offset+j+1; //colon also takes a byte
			++count;
			//reset
			j = 0;
		}
		//in case of one argument only
		if(count==0) memcpy((void*)_packet->type,buffer,j);
		else perror("read_buffer too few args");
	}
}


int main(int argc, char** argv){

	while(1){
		char cmd[100]={0};
		char type[20]={0};
		fgets(cmd, sizeof(cmd), stdin);
		sscanf(cmd, "%s", type);

		if(strcmp(type,"login")==0){
			char id[10],pw[10], ip[16],port[2];
			int err = sscanf(cmd,"%s %s %s %s %s",type, id,pw,ip,port);
			printf("%s %s %s %s\n",type, id, ip,port);
			if(err<0) continue;


			struct addrinfo hints, *res,*iter;
			memset(&hints, 0, sizeof(hints));
			memset(&res, 0, sizeof(res));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM; //use TCP
	
			err = getaddrinfo(ip, port, &hints, &res);
			if(err<0){
				fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
				return 1;
			}
			if(!res) printf("null res\n");
		
			//loop through all returned addresses and connect to the first we can
			int sockfd;
			for(iter=res; iter!=NULL;iter=res->ai_next){
				sockfd = socket(iter->ai_family, iter->ai_socktype,
						iter->ai_protocol);
				if(sockfd == -1){
					printf("socket\n");
					continue;
				}

				if(connect(sockfd,iter->ai_addr, iter->ai_addrlen)<0){	
					close(sockfd);
					printf("client: connect\n");
					continue;
				}
				break;
			}
			//all done with this structure
			freeaddrinfo(res);

			if(!iter){
				fprintf(stderr, "connect fails\n");
				return 1;

			}


			char buf[100];
			bzero(buf,sizeof(buf));
			sprintf(buf,"LOGIN:%d:%s:%s",10,id,pw);
			send(sockfd,buf,100,0);

			struct packet _p;
			bzero(buf,100);
			bzero(&_p,sizeof(struct packet));
			recv(sockfd,buf,100,0);
			read_buffer(buf,&_p);
			int check = strcmp(_p.type,"LO_ACK");
			if(check==0) printf("login succeed\n");
			else printf("%s\n",_p.type);
		}
		

	}
}
