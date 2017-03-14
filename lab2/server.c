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
#define lenname 10 //MAX_NAME
#define lenpass 10

//hard-coded userid and password
char username[1][lenname]={"hamid"};
char password[1][lenpass]={"hm123"};


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
void error_timeout(int sockfd);


int main(int argc, char** argv){
	if(argc!=2){
		fprintf(stderr, "usage: server <TCP listen "
		"port>\n");
		return 1;
	}

	memset(sessions, 0, sizeof(struct room)*maxsessions);
	pthread_t threads[nthreads];
	//divide parallal workload
	struct args t_arg[4];

	int i, err;
	for(i=0; i<nthreads; ++i){

	t_arg[i].num_sessions=maxsessions/nthreads;
	t_arg[i].list=sessions+i*t_arg[i].num_sessions;
		t_arg[i].pid=i;
		err=pthread_create(threads+i,NULL,session_begin,(void*)&t_arg[i]);

		if(err){
			perror("pthread_create");
			exit(-1);
		}

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



while(1)
;
printf("thread %d return\n",arg->pid);
	return 0;
}


//id authetication
//id and password are maxmimum 10 bytes each
int authorize_usr(int sockfd){
	//set timeout to the following recv call.
	//a round trip time is about 200ms
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 400000;
	int err = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
		&timeout, sizeof(timeout));
	
	int done=0, count=0;
	char buf[100];

	while(!done && count<3){
		memset(buf,0,100);
		err = recv(sockfd,(void*)buf,100,0); 

		if(errno==EAGAIN){ //times out
			error_timeout(sockfd);
			return done;
		}
	
		char type[20];
		int numB = sscanf(buf,"%s",type);
		if(strcmp(type,"LOGIN")==0){
			char id[lenname],pass[lenpass];
			sscanf(buf+count, "%s %s",id,pass);
			//check id with password
	
	
		}else{
			//wrong id or password
			++count;
			char msg[]="LO_NAK:wrong username or password";
			send(sockfd, msg, strlen(msg),0);
			continue;	
		}
	}
	if(!done){
		//authetication fails
		char msg[]="LO_NAK:wrong username or password";
		send(sockfd, msg, strlen(msg),0);
	}

	return done;
}

void read_request(int sockfd){


}

void error_timeout(int sockfd){
	char buf[8]="TIMEOUT";
	send(sockfd,(void*)buf,strlen(buf),0);
	close(sockfd);
	return;
}

