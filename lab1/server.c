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

#define MAXBUFLEN 2000
//discomment this to see the time out effect on the deliver side
//#define DEBUG_TIMEOUT

//c does not support passing by reference
void write_to_local(int socket);
int read_buffer(char [], int [], char []);
void print_sock(struct sockaddr* s_addr);


int main(int argc, char** argv){
	int sockfd;
	int err;
	struct addrinfo hints, *servinfo, *checkinfo;
	char buffer[200]; //Q7: what if it's a ptr instead of actual mem?

	if(argc!=2){
		fprintf(stderr, "usage: server <UDP listen "
		"port>\n");
		return 1;
	}

	//Initialize server address info
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET; //use IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; //use my ip

	//Fill in address struct
	err = getaddrinfo(NULL, argv[1], &hints, &servinfo);
	if(err!=0){
		fprintf(stderr, "getaddrinfo fails,"
		" error: %s\n",gai_strerror(err));
		return 1;
	}

	//Loop until find a valid port or reach to the end
	for(checkinfo = servinfo; checkinfo != NULL; checkinfo = checkinfo->ai_next){

		//create socket
		sockfd = socket(checkinfo->ai_family,
			checkinfo->ai_socktype,
			checkinfo->ai_protocol);
		//Q5: what can cause socket() fail? in use?
		if(sockfd == -1){
			perror("socket");
			continue;
		}

/*		//bind this socket
		int yes = 1;
		err = setsockopt(sockfd, SOL_SOCKET,
			SO_REUSEADDR, &yes, sizeof(yes));
		if(!err){
			perror("setsockopt");
			exit(1);
		}
*/
		if(bind(sockfd, checkinfo->ai_addr, checkinfo->ai_addrlen)==-1){
			close(sockfd);
			perror("bind");
			continue;
		}

		break;
	}

	if(!checkinfo){
		fprintf(stderr, "no valid port!\n");
		return 2;
	}

	//Q3: free doesn't close connection for TCP?
	//free can/should be placed right after successful call of bind?
	//what about before exit()?
	freeaddrinfo(servinfo);


	while(1){
		printf("listening to receive new file\n");
	
		//read message
		//Q4: use a while loop to keep reading? or reopen a socket
		struct sockaddr src_addr;
		socklen_t addrlen = sizeof(src_addr);
		bzero(&src_addr, sizeof(src_addr));
		int readbytes = recvfrom(sockfd, buffer, 200, 0,
				&src_addr, &addrlen);
	 	if(readbytes == -1){
			perror("recvfrom");
			exit(1);
		}
		//Q6: is msg a c string?(ended with \0 in itself?
		//I see some code that adds a '\0' to the received msg
		//printf("%s\n",buffer);
	
		char response[10] = "no";
		if(strcmp(buffer,"ftp") == 0) strcpy(response, "yes");
	
		sendto(sockfd, response, sizeof(response), 0,
			&src_addr, addrlen);
	
		//Now: single thread server
		//Todo: create worker threader
		write_to_local(sockfd);
		//Finish writing the file
	
	}

	close(sockfd);

	return 0;	
}



void write_to_local(int socket){
	char buffer[MAXBUFLEN];
	//0-total_frag, 1-frag_no, 2-size,
	int header_info[3]= {0};
	char filename[200];
	bzero(filename, 200);
	int data_start=0, openfile=1, count_packet=0;
	//array to keep track which packet has been received
	int *packet_received;
	FILE *fp = NULL;

	//For pasrt 3, keep tracking the previous sequence number and send ACK/NACK
	int last_fragNo = 0;

	do{
		struct sockaddr new_source;
		socklen_t addrlen = sizeof(new_source);
		#ifdef DEBUG_TIMEOUT
		printf("receiving...\n");
		#endif

		if(recvfrom(socket, buffer, MAXBUFLEN, 0,
			&new_source, &addrlen) == -1){
			perror("recvfrom--write to local");
			if(fp!=NULL) {
				fclose(fp);
				free(packet_received);
			}
			exit(1);
		}

		#ifdef DEBUG_TIMEOUT
		printf("processing...\n");
		//print_sock(&new_source);
		#endif	

		/* read the packet into buffer */
		data_start = read_buffer(buffer, header_info, filename);

		//if not the expected packet structure
		//return and listen to new connection	
		if(strcmp(filename,"") == 0 || header_info[0] <= 0 ||
			header_info[1] <= 0 || header_info[2] < 0){
				if(fp!=NULL){
					fclose(fp);
					free(packet_received);
				}
				return;
			}
	
		//Initialize the file
		//openfile is the flag, the first time enter this loop
		if(openfile){
			char filepath[200]="./cookie/";
			strcpy((filepath+9), filename);

			fp = fopen(filepath, "w");
			if(!fp) perror("fopen");
			openfile = 0;
	
			//the total data size - the last packet size
			long int file_size = (header_info[0] - 1);
			if(file_size > 0){
				file_size = 1000*file_size*sizeof(char);
				fseek(fp, file_size, SEEK_END);
//				fputc('\0', fp); 
			}
			packet_received = calloc(header_info[0], sizeof(int));
		}


		/**  ACK/NACK **/
		char response[4] = "NACK"; //by default

		//ACK iff the received packet has been received before
		//or its the direct subscequent packet of the last one.
		if ((header_info[1] - last_fragNo == 1) ||
			(packet_received[header_info[1]-1] == 1)){
		#ifndef DEBUG_TIMEOUT
			//send ACK
			sendto(socket, &response[1], sizeof(char)*3, 0,
				&new_source, addrlen);
			printf("ACK\n");
		#endif

			//increment the last frag no
			if(!packet_received[header_info[1]-1])
			last_fragNo++;

		}else{
			//send back NACK
			sendto(socket, response, sizeof(char)*4, 0,
				&new_source, addrlen);

			printf("NACK\n");
			//continue listening
			continue;
		}


		//if this packet has been read, skip
		if(packet_received[header_info[1]-1] != 0) continue;
		//this packet is read, mark it
		packet_received[header_info[1]-1] = 1;
	
	
		printf("pid %d/%d\n",header_info[1], header_info[0]);
		//if this is the last packet
		if(header_info[1] == header_info[0]){
			//wired, fwrite can't write to eof??
			//I have to seek forwards before write
			fseek(fp, 0L, SEEK_END);
			long int current = ftell(fp);
			long int offset = header_info[2]*sizeof(char);
			fseek(fp, offset, SEEK_CUR);
			fseek(fp, current, SEEK_SET); //go back

			fwrite((buffer+data_start), sizeof(char),
				header_info[2], fp);
	
		}
		else{
			if(header_info[2]!=1000){
				fprintf(stderr, "data size is %d (should "
					"be 1000)\n", header_info[2]);
				exit(1);
			}
			long int offset = (header_info[1]-1)*1000*sizeof(char);
			fseek(fp, offset, SEEK_SET); 
			fwrite((buffer+data_start), sizeof(char), 1000, fp);
		}
		count_packet++;
	
		}while(count_packet != header_info[0]);
		printf("file has been saved to ./cookie/%s\n", filename);
		printf("received %d packets, expected %d packets\n",count_packet, header_info[0]);
		fclose(fp);
}




//return the offset at which the actual data starts in the buffer
int read_buffer(char buffer[], int header_info[3], char filename[200]){

	unsigned int fake_len = strlen(buffer);
	int i=0, j=0, count=0;
	for(i = 0; i<fake_len; ++i){
		if(buffer[i] != ':'){
			filename[j] = buffer[i];
			++j;
		}else{
			if(count==3) break;
			header_info[count] = atoi(filename);
			++count;
			//reset
			++j;
			bzero(filename, j);
			j = 0;
		}


	}

	return (i+1);
}

void print_sock(struct sockaddr* s_addr){
	printf("Address Family: %d\n", s_addr->sa_family);
	if(s_addr->sa_family == AF_INET){
		char dst[14];
		inet_ntop(AF_INET,&(((struct sockaddr_in*)s_addr)->sin_addr), dst, 14);
		printf("IP addr: %s\n", dst);
		printf("port number: %d\n", ((struct sockaddr_in*)s_addr)->sin_port);

	}
}
