//Fan Xie
//2017-03-15
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
int active_ss[numusers]; //track the user's current session
fd_set readfds; //set of user socket descriptors
int max_sockfd; //the highest-numbered socket descriptor  
int master_fd; //new connection socket


/*** data section ***/
//hard-coded userid and password
char const username[numusers][lenname]={"Hamid","Dory","Mary","Prof.B"};
char const password[numusers][lenpass]={"hm123","finding101","christmas","matrix!"};



/** functions ***/
void session_begin();
void error_timeout(int sockfd);
void read_buffer(char buffer[], struct packet*);
int authorize_user(int sockfd);
int create_session(int uindex);
struct room* create_room(int uindex,int sid);
struct room* delete_room(struct room* _r,struct room **head);
void leave_all_sessions(int uindex);


//given user name, return the user id
int find_user(char *name){
	int i;
	for(i=0;i<numusers;++i){
		if(strcmp(username[i],name)==0){
			return i;
		}
	}
	return -1;
}

//return 1 if user uid is in the session ssid
//0 otherwise
int joined_ss(int uid, int ssid){
	struct room *cur=sessions[ssid];
	while(cur!=NULL){
		if(cur->uid==uid)
			return 1;
		cur=cur->next;
	}
	return 0;
}


//update active session for user uid to the first session found in the list
void update_ss(int uid){
	int i;
	active_ss[uid]=-1;
	for(i=0;i<maxsessions;++i){
		struct room* cur=sessions[i];
		while(cur!=NULL){
			if(cur->uid==uid){
				active_ss[uid]=i;
				return;
			}
			cur=cur->next;
		}
	}
}

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
				send(active_user[cur->uid],buf,len,0);
			cur=cur->next;
		}
	}

}

//send message to all users in the same active session
void send_session(char *buf, int uindex, int len){
	if(active_ss[uindex]<0) return;
	int ssid = active_ss[uindex];
	struct room* cur=sessions[ssid];
	while(cur!=NULL){
		if(cur->uid!=uindex && (ssid==active_ss[cur->uid]))
			send(active_user[cur->uid],buf,len,0);

		cur=cur->next;	
	}
}


void reset_max_socket(){
	max_sockfd=master_fd;

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
				cur = delete_room(cur,&sessions[i]);
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
			delete_room(cur,&sessions[ssid]);
			ss_ref[ssid]--;
			assert(ss_ref[ssid]>=0);
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
struct room* delete_room(struct room* _r, struct room** head){
	if(!_r) return NULL;
	if(_r->prev!=NULL)
		_r->prev->next=_r->next;
	else //this is the head
		*head = _r->next; //pass the head to the next in the list
	if(_r->next!=NULL)
		_r->next->prev=_r->prev;
	struct room* ret;
	ret = _r->next;
	_r->next=NULL;
	_r->prev=NULL;
	free(_r);
	return ret;
}

int query_us(char *buf,int uid){
	int i=0,offset=0;
	for(;i<maxsessions;++i){
		struct room* cur=sessions[i];
		while(cur!=NULL){
			if(cur->uid == uid && cur->session_id == active_ss[uid]){
				offset+=sprintf(buf+offset,"<%s,%d> <--- current active session\n",
					username[cur->uid],cur->session_id);
			}else
				offset+=sprintf(buf+offset,"<%s,%d>\n",
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
	char buf[2000]={0};
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
		active_ss[i]=-1;
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
	master_fd = sockfd;
	printf("server: waiting for connections...\n");


	//keep receving new connections
	while(1){


		FD_ZERO(&readfds); //clear all entries from the set
		FD_SET(master_fd,&readfds); //add the listening socket to the read set

		max_sockfd = master_fd;
		for(i=0;i<numusers;++i){
			if(active_user[i]!=-1){
				//add the connected user socket to the set
				FD_SET(active_user[i],&readfds);

				if(active_user[i]>max_sockfd)
					max_sockfd=active_user[i];
			}
		}


		//timeval is null to wait indefinitely
		int err=select(max_sockfd+1,&readfds,NULL,NULL,NULL);
		if(err<0) perror("select");

		if(FD_ISSET(master_fd,&readfds)){
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
		else{
			//new data coming in
			session_begin();
		}

	}

	return 0;
}


//worker thread processess all user commands other than "login"
void session_begin(){
	int i;

	#ifdef DEBUG
	printf("session begins\n");
	#endif
	for(i=0;i<numusers;++i){
		if(active_user[i]==-1) continue;
		if(FD_ISSET(active_user[i],&readfds)){
			char temp_buf[lendata*2];
			struct packet _p;
			int fd=active_user[i];
			memset(temp_buf,0,lendata*2);
			memset(&_p,0,sizeof(struct packet));

			recv(fd,temp_buf,lendata*2,0);
			read_buffer(temp_buf,&_p);

			//close the connected socket when user suddenly quits
			if(strcmp(username[i],_p.uid)!=0){
				printf("user %s is lost. Closing the socket\n",username[i]);
				//user log out
				leave_all_sessions(i);
				active_ss[i]=-1;
				FD_CLR(fd, &readfds);
				close(active_user[i]);
				active_user[i]=-1;
				reset_max_socket();
				return;
			}

			if(strcmp(_p.type,"EXIT")==0){
				//user log out
				leave_all_sessions(i);
				active_ss[i]=-1;
				FD_CLR(fd, &readfds);
				//close the socket
				close(active_user[i]);
				active_user[i]=-1;
				reset_max_socket();
				printf("user %s exits\n",username[i]);

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
					//update active session
					active_ss[i]=ssid;
					char notice[100];
					int bytes=sprintf(notice,"MESSAGE:%s has joined the session",_p.uid);
					send_session(notice,i,bytes);
				}
				send(fd,reply,strlen(reply),0);

			}else if(strcmp(_p.type,"LEAVE_SESS")==0){
				int ssid = atoi(_p.data);
				if(ssid<0 || ssid>= maxsessions) return;

				char notice[100];
				int bytes=sprintf(notice,"MESSAGE:%s has left the session",_p.uid);
				send_session(notice,i,bytes);

				if(ss_ref[ssid]>0)
					leave_session(i,ssid);
				//update the active session
				if(ssid == active_ss[i])
					update_ss(i);

			}else if(strcmp(_p.type,"NEW_SESS")==0){
				char reply[50];
				int ssid=create_newsession(i);	
				if(ssid<0){
					sprintf(reply,"NS_NACK:new session fails");
				}
				else{
					//update the active session of this user
					active_ss[i] = ssid;
					sprintf(reply,"NS_ACK:%d",ssid);
				}
				send(fd,reply,100,0);
				
			}else if(strcmp(_p.type,"MESSAGE")==0){
				char reply[lendata+8];
				int len = sprintf(reply,"MESSAGE:%s-> ",_p.uid);
				memcpy(reply+len,_p.data,_p.size*sizeof(char));
				len+=_p.size;
				//broadcast_sessions(reply,i,len);
				//send message into the current session
				send_session(reply,i,len);
			
			}else if(strcmp(_p.type,"QUERY")==0){
				char reply[lendata+7]; //initialize to 0?
				sprintf(reply,"QU_ACK:");
				int err = query_us(reply+7,i);
				if(err<-1)
					fprintf(stderr,"query buffer overflows\n");
				else
					send(fd,reply,lendata+7,0);
			}else if(!strcmp(_p.type,"SWITCH")){
				//switch the current active session
				int ssid = atoi(_p.data);
				int len=0;
				if(ssid<0 || ssid>= maxsessions) return;

				char reply[100]={0};
				if(!joined_ss(i,ssid)){
					//not joined this session yet
					len=sprintf(reply,"SW_NACK:You are not in this session yet\n"
							"Request declined\n");
				}else{
					len=sprintf(reply,"SW_ACK:Success\n");
					active_ss[i]=ssid;
				}
					send(fd,reply,len,0);

			}else if(!strcmp(_p.type,"INVITE")){
				//invite another user to his/her current active session
				if(active_ss[i]<0) {perror("unexpected error!"); exit(1);}
				char msg[100]={0};

				int invitee = find_user(_p.data);
				//printf("inviting user %s\n",_p.data);
				//printf("found user id is %d\n",invitee);

				if(invitee<0 || invitee >= numusers) return;
				int len = sprintf(msg,"INVITEE%d:%s invites you to join session %d",active_ss[i],_p.uid,active_ss[i]);
				send(active_user[invitee],msg,len,0);

			}else if(!strcmp(_p.type,"INV_REPLY")){
				//reply should be in the format: yes/no ssid
				int ssid=-1;
				char ans[4]={0};
				sscanf(_p.data,"%s %d",ans,&ssid);
				if(strcmp(ans,"no")==0){ return;}
				if(strcmp(ans,"yes")==0){
					if(!joined_ss(i,ssid)){
						join_session(i,ssid);
						//update active session
						active_ss[i]=ssid;
						char notice[100];
						int bytes=sprintf(notice,"MESSAGE:%s is invited to the "
								"session\n", _p.uid);
						send_session(notice,i,bytes);
					}
					//else user is already in the session
					char reply[100]={0};
					int len=sprintf(reply,"REPLY_ACK:You are in the session %d\n",ssid);
					send(fd,reply,len,0);
				}
				
				//printf("INV_REPLY:\n%s\n",_p.data);
			}

			break;
		}
	}
}


