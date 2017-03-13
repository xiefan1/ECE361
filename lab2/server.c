#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#define BACKLOG 10 //the size of listening queue
#define nthreads 4 //number of data threads
#define maxsessions 20
#define DEBUG

//linked list to store users in a session
struct room{
	int sockfd; //connection b/t the server and client
	int session_id;
	struct room* next;
};
struct args{
	struct room *list;
	int num_sessions;
	int pid;
};

/*** global variables ***/
//array of linked lists
struct room sessions[maxsessions]; //max available sessions are 50
int s_counter[maxsessions]={0}; //keep tracking #users in a session

/** functions ***/
void* session_begin(void *);


int main(int argc, char** argv){
	if(argc!=2){
		fprintf(stderr, "usage: server <TCP listen "
		"port>\n");
		return 1;
	}

	memset(sessions, 0, sizeof(struct room)*maxsessions);
	pthread_t threads[nthreads];
	//divide parallal workload
	struct args t_arg;
	t_arg.list=sessions;
	t_arg.num_sessions=maxsessions/nthreads;

	int i, err;
	for(i=0; i<nthreads; ++i){

		t_arg.pid=i;
		err=pthread_create(threads+i,NULL,session_begin,(void*)&t_arg);
		printf("main pid %d, %d\n",i, (int)(t_arg.list-sessions));

		if(err){
			perror("pthread_create");
			exit(-1);
		}

		t_arg.list+=t_arg.num_sessions;
	}

	struct addrinfo hints,*res, *iter;

	bzero(&hints, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM; //use TCP
	//?hints.ai_protocol = 0;

	err = getaddrinfo(NULL, argv[1], &hints, &res);
	if(err<0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		return 1;
	}

	//loop through all returned addresses and bind to the first we can
	//the control thread owns sockfd
	int sockfd;
	for(iter=res; iter!=NULL;iter=res->ai_next){
		sockfd = socket(iter->ai_family, iter->ai_socktype,
				iter->ai_protocol);
		if(sockfd == -1){
			perror("socket");
			continue;
		}

		//set socket options
		int yes =1;
		if(setsockopt(sockfd, SOL_SOCKET,SO_BROADCAST,
				&yes, sizeof(yes))==-1){
			close(sockfd);
			perror("bind");
			continue;
		}

		if(bind(sockfd, iter->ai_addr, iter->ai_addrlen)==-1){
			close(sockfd);
			perror("bind");
			continue;
		}
		break;
	}
	//all done with this structure
	freeaddrinfo(res);

	if(!iter){
		fprintf(stderr, "bind fails\n");
		return 1;

	}

	if(listen(sockfd, BACKLOG)==-1){
		close(sockfd);
		perror("listen");
		return 1;
	}

	printf("server: waiting for connections...\n");

	while(1){
		struct sockaddr new_addr;
		socklen_t new_addrlen;

		int new_sockfd = accept(sockfd,&new_addr, &new_addrlen);
		if(new_sockfd<0){
			perror("accept");
			continue;
		}

		//id authetication


	}

	return 0;
}


void* session_begin(void * voidData){
	struct args* arg = voidData;
	struct room *list = arg->list;
	int size = arg->num_sessions;

	printf("session begins at %d %d\n",(int)(arg->list-sessions),arg->pid);





	return 0;
}

void read_request(int sockfd){
	//set timeout to the following recv call.
	//a round trip time is about 200ms
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 400000;
	int err = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
		&timeout, sizeof(timeout));
	
	char buf[100]={0};
	err = recv(sockfd,(void*)buf,100,0); 
	if(errno==EAGAIN) return; //times out

	char type[20];
	int count = sscanf(buf,"%s",type);
	if(strcmp(type,"LOGIN")==0){
		//id authetication
		char id[10],pass[10];
		sscanf(buf+count, "%s %s",id,pass);



	}


}
