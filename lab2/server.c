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
#define nthreads 1 //number of data threads
#define maxsessions 20
#define lenname 10 //MAX_NAME
#define lenpass 10
#define lendata 1024
#define numusers 4

#define DEBUG



//linked list to store users in a session
struct room{
	int sockfd; //connection b/t the server and client
	int uid; //user name
	int session_id;
	struct room* next;
};

struct args{
	struct room *list;
	int num_sessions;
	int pid;
};

struct packet{
	char type[10];
	unsigned int size;
	char uid[lenname];
	char data[lendata];
};


/*** global variables ***/
//array of linked lists
struct room sessions[maxsessions]={0}; //max available sessions are 50
int s_counter[maxsessions]={0}; //keep tracking #users in a session
int active_user[numusers]; //keep track active users, index is the key, user-socket is the value


/*** data section ***/
//hard-coded userid and password
char const username[numusers][lenname]={"Hamid","Dory","Mary","Prof.B"};
char const password[numusers][lenpass]={"hm123","finding101","christmas","matrix!"};



/** functions ***/
void* session_begin(void *);
void error_timeout(int sockfd);
void read_buffer(char buffer[], struct packet*);
int authorize_user(int sockfd);

//id authetication, return 1 on success, o therwise
//on success, this function adds the user index and sockfd into active_user
int authorize_user(int sockfd){
	const int maxlen=2000;
	char buf[maxlen]={0};
	struct packet _packet;
	memset(&_packet,0,sizeof(_packet));

	//set timeout to the following recv call.
	//a round trip time is about 200ms
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 400000;
	int err = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
		&timeout, sizeof(timeout));

	err = recv(sockfd,(void*)buf,maxlen,0); 

	if(errno==EAGAIN){ //times out
		error_timeout(sockfd);
		return 0;
	}

	read_buffer(buf,&_packet);

	if(strcmp(_packet.type,"LOGIN")==0){
		//check id with password
		int i=0;
		for(;i<numusers;++i){
			if(strcmp(username[i],_packet.uid)==0){
				if(strcmp(password[i],_packet.data)==0){
					if(active_user[i]==-1){	
						active_user[i]=sockfd;
						char msg[]="LO_ACK";
						send(sockfd,msg,strlen(msg),0);
						printf("welcome new user %s\n",_packet.uid);
						return 1;
					}
					//this user has logged in
					char msg[]="LO_NACK:user has logged in";
					send(sockfd,msg,strlen(msg),0);
					return 0;
				}}}
	}

	//authetication fails
	char msg[]="LO_NAK:wrong username or password";
	send(sockfd, msg, strlen(msg),0);

	return 0;
}


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
			else if(count==1){
				char temp[2];
				memcpy(temp,buffer+offset,j);
				_packet->size = atoi(temp);
			}else if(count==2){
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
	}
}

void error_timeout(int sockfd){
	char buf[7]="TIMEOUT";
	send(sockfd,(void*)buf,strlen(buf),0);
	return;
}


int main(int argc, char** argv){
	if(argc!=2){
		fprintf(stderr, "usage: server <TCP listen "
		"port>\n");
		return 1;
	}

	/** data initialization**/
	memset(sessions, 0, sizeof(struct room)*maxsessions);
	int i,err;
	for(i=0;i<numusers;++i){
		active_user[i]=-1;
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
	int sockfd;
	for(iter=res; iter!=NULL;iter=res->ai_next){
		sockfd = socket(iter->ai_family, iter->ai_socktype,
				iter->ai_protocol);
		if(sockfd == -1){
			perror("socket");
			continue;
		}

		//set socket options
		/*
		int yes =1;
		if(setsockopt(sockfd, SOL_SOCKET,SO_BROADCAST,
				&yes, sizeof(yes))==-1){
			close(sockfd);
			perror("bind");
			continue;
		}*/

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


	/** create worker thread **/
	pthread_t threads[nthreads];
	struct args t_arg[nthreads];

	for(i=0; i<nthreads; ++i){

		//divide parallal workload
		t_arg[i].num_sessions=maxsessions/nthreads;
		t_arg[i].list=sessions+i*t_arg[i].num_sessions;
		t_arg[i].pid=i;

		err=pthread_create(threads+i,NULL,session_begin,(void*)&t_arg[i]);

		if(err){
			perror("pthread_create");
			exit(-1);
		}

	}


	//keep receving new connections
	while(1){
		struct sockaddr new_addr;
		socklen_t new_addrlen;

		int new_sockfd = accept(sockfd,&new_addr, &new_addrlen);
		if(new_sockfd<0){
			perror("accept");
			continue;
		}

		//id authetication
		//this function adds user and new_sockfd into active_user
		if(!authorize_user(new_sockfd))
			close(new_sockfd);

	}

	return 0;
}


void* session_begin(void * voidData){
	struct args* arg = voidData;
	struct room *list = arg->list;
	int size = arg->num_sessions;

	#ifdef DEBUG
	printf("session begins at %d %d\n",(int)(arg->list-sessions),arg->pid);
	int i=0;
	for(;i<numusers;++i){
	if(active_user[i]!=-1)
	printf("active user: %s\n", username[i]);
	}
	#endif


	return 0;
}



