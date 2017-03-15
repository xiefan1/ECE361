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
#include <sys/select.h>
#include <assert.h>

#define BACKLOG 10 //the size of listening queue
#define maxsessions 20
#define lenname 10 //MAX_NAME
#define lenpass 10
#define lendata 1024
#define numusers 4

#define DEBUG



//linked list to store users in a session
struct room{
	int sockfd; //connection b/t the server and client
	int uid; //user idex
	int session_id;
	struct room* prev;
	struct room* next;
};


struct packet{
	char type[10];
	unsigned int size;
	char uid[lenname];
	char data[lendata];
};


/*** global variables ***/
//array of linked lists
struct room* sessions[maxsessions]; //max available sessions are 50
int ss_ref[maxsessions]={0}; //keep tracking #users in a session
int active_user[numusers]; //keep track active users, index is the key, user-socket is the value
fd_set readfds; //set of user socket descriptors
int max_sockfd=-1; //the highest-numbered socket descriptor  


/*** data section ***/
//hard-coded userid and password
char const username[numusers][lenname]={"Hamid","Dory","Mary","Prof.B"};
char const password[numusers][lenpass]={"hm123","finding101","christmas","matrix!"};



/** functions ***/
void* session_begin();
void error_timeout(int sockfd);
void read_buffer(char buffer[], struct packet*);
int authorize_user(int sockfd);
int create_session(int uindex);
struct room* create_room(int uindex,int sid);
struct room* delete_room(struct room* _r);
void leave_all_sessions(int uindex);

//this broad casts to all sessions uid is in
void broadcast_sessions(char *buf,int uindex, int len){
	int i=0,count=0;

	//find all sessions this user is in
	int pending_sessions[maxsessions];
	for(i=0;i<maxsessions;++i){
		struct room* cur=sessions[i];
		while(cur!=NULL){
			if(cur->uid==uindex){
				pending_sessions[count]=i;	
				++count;
				break;
			}
			cur=cur->next;
		}
	}

	//send the message
	for(i=0;i<count;++i){
		struct room* cur=sessions[i];
		while(cur!=NULL){
			if(cur->uid!=uindex) 
				send(active_user[cur->uid],buf,len,0);;
			cur=cur->next;
		}
	}

}

void reset_max_socket(){
	max_sockfd=-1;

	int i=0;
	for(;i<numusers;++i){
		if(active_user[i]>max_sockfd)
			max_sockfd=active_user[i];
	}
}


void leave_all_sessions(int uindex){
	int i=0;
	for(;i<maxsessions;++i){
		struct room* cur=sessions[i];
		while(cur!=NULL){
			if(cur->uid==uindex){
				cur = delete_room(cur);
				ss_ref[i]--;
				assert(ss_ref[i]>=0);
			}
			else cur=cur->next;
		}
	}
}

//this function allocate a new session to the user and return the session id
int create_newsession(int uindex){
	int i=0;
	for(;i<maxsessions;++i){
		if(ss_ref[i]!=0) continue;

		ss_ref[i]+=1;
		sessions[i]=create_room(uindex,i);
		return i;
	}
	return -1;
}

struct room* create_room(int uindex,int sid){
	struct room* ret=malloc(sizeof(struct room));
	//Todo: lock
	ret->sockfd = active_user[uindex];
	ret->uid = uindex;
	ret->session_id = sid;
	ret->prev=NULL;
	ret->next=NULL;
	return ret;
}

void leave_session(int userid, int ssid){
	struct room* cur=sessions[ssid];
	while(cur!=NULL){
		if(cur->uid==userid){
			delete_room(cur);
			ss_ref[ssid]--;
			assert(ss_ref[ssid]>=0);
			return;
		}
		cur=cur->next;
	}
}


void join_session(int userid, int ssid){
	struct room* cur=sessions[ssid];
	struct room* user = create_room(userid,ssid);
	cur->prev=user;
	user->next=cur;;
	sessions[ssid]=user;
	ss_ref[ssid]++;
}

//delete _r and return the next struct room pointed by _r
struct room* delete_room(struct room* _r){
	if(!_r) return NULL;
	if(_r->prev!=NULL)
		_r->prev->next=_r->next;
	struct room* ret;
	ret = _r->next;
	_r->next=NULL;
	_r->prev=NULL;
	free(_r);
	return ret;
}

int query_us(char *buf){
	int i=0,offset=0;
	for(;i<maxsessions;++i){
		struct room* cur=sessions[i];
		while(cur!=NULL){
			offset=sprintf(buf+offset,"<%s,%d>\n",
				username[cur->uid],cur->session_id);

			cur=cur->next;
		}
	}
	if(offset>lendata)
		return -1;
	else return 0;
}

//id authetication, return 1 on success, o therwise
//on success, this function adds the user index and sockfd into active_user
//and add this sockfd into select set
int authorize_user(int sockfd){
	const int maxlen=2000;
	char buf[maxlen]={0};
	struct packet _packet;
	memset(&_packet,0,sizeof(_packet));

	//set timeout to the following recv call.
	//a round trip time is about 200ms
	struct timeval timeout;
	timeout.tv_sec = 60;
	timeout.tv_usec = 0;
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
						//Todo: can add a lock here
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
	int i,err;
	for(i=0;i<maxsessions;++i){
		sessions[i]=NULL;
		ss_ref[i]=0;
	}
	for(i=0;i<numusers;++i){
		active_user[i]=-1;
	}

	FD_ZERO(&readfds); //clear all entries from the set


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
	pthread_t data_thread;
	err=pthread_create(&data_thread,NULL,session_begin,NULL);
	if(err){
		perror("pthread_create");
		exit(-1);
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
		else{
			//user connected
			//Todo: need lock here
			FD_SET(new_sockfd, &readfds);
			if(new_sockfd>max_sockfd) max_sockfd=new_sockfd;
		}

	}

	return 0;
}


//worker thread processess all user commands other than "login"
void* session_begin(void * voidData){
	int i;

	#ifdef DEBUG
	printf("session begins\n");
	#endif

	while(1){
		//listen on the user sockfds
		//timeval is null to wait indefinitely
		int err=select(max_sockfd+1,&readfds,NULL,NULL,NULL);
		if(err<0) perror("select");
	
		for(i=0;i<numusers;++i){
			if(active_user[i]==-1) continue;
			if(FD_ISSET(active_user[i],&readfds)){
				char temp_buf[lendata*2];
				struct packet _p;
				int fd=active_user[i];
				memset(temp_buf,0,lendata*2);

				recv(fd,temp_buf,lendata*2,0);
				read_buffer(temp_buf,&_p);

				assert(strcmp(username[i],_p.uid)==0);

				if(strcmp(_p.type,"EXIT")==0){
					//user log out
					leave_all_sessions(i);
					FD_CLR(fd, &readfds);
					active_user[i]=-1;
					reset_max_socket();
					printf("user %s exists\n",username[i]);

				}else if(strcmp(_p.type,"JOIN")==0){
					int ssid = atoi(_p.data);
					char reply[100]={0};
					if(ssid<0 || ssid>= maxsessions)
						sprintf(reply,"JN_NACK:session %d out of "
							"bound (0,%d)",ssid,maxsessions-1);
					else if(ss_ref[ssid]==0)
						sprintf(reply,"JN_NACK:session %d does not "
							"exist.",ssid);
					else{
						sprintf(reply,"JN_ACK");
						join_session(i,ssid);
					}
					send(fd,reply,strlen(reply),0);

				}else if(strcmp(_p.type,"LEAVE_SESS")==0){
					int ssid = atoi(_p.data);
					if(ssid<0 || ssid>= maxsessions) break;

					if(ss_ref[ssid]>0)
						leave_session(i,ssid);

				}else if(strcmp(_p.type,"NEW_SESS")==0){
					char reply[50];
					int ssid=create_newsession(i);	
					if(ssid<0){
						sprintf(reply,"NS_NACK:new session fails");
					}
					else{
						sprintf(reply,"NS_ACK:%d",ssid);
					}
					send(fd,reply,100,0);
					
				}else if(strcmp(_p.type,"MESSAGE")==0){
					char reply[lendata+8];
					int len = sprintf(reply,"MESSAGE:");
					memcpy(reply+len,_p.data,_p.size*sizeof(char));
					len+=_p.size;
					broadcast_sessions(reply,i,len);
				
				}else if(strcmp(_p.type,"QUERY")==0){
					char reply[lendata+7];
					sprintf(reply,"QU_ACK:");
					int err = query_us(reply+7);
					if(err<-1)
						fprintf(stderr,"query buffer overflows\n");
					else
						send(fd,reply,lendata+7,0);
				}

				break;
			}
		}
	}

	return 0;
}


