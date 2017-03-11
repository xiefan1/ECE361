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

#define TIMER
#define DEBUG
#define MAXDATALEN 1000

int main(int argc, char** argv){

	if(argc != 3){
		fprintf(stderr, "bad usage: deliver <server address> "
			"<server port number>\n");
		return 1;
	}


	//get destination address info
	int sockfd;
	int err;
	struct addrinfo hints, *res, *checkinfo;;
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET; //IPv4
	hints.ai_socktype = SOCK_DGRAM;
	if(getaddrinfo( argv[1], argv[2], &hints, &res) != 0){
	//Q1: whose addrinfo is returned?
	//dest, but it's used to open client's socket??
		perror("getaddrinfo");
		return 1;
	}

	//loop until find a valid socket or reach the end
	for(checkinfo = res; checkinfo != NULL; checkinfo = checkinfo->ai_next){
		sockfd = socket(checkinfo->ai_family,
			checkinfo->ai_socktype, checkinfo->ai_protocol);
		if(sockfd == -1){
			perror("socket");
			continue;
		}

		break;
	}

	if(!checkinfo){
		fprintf(stderr,"socket connection fails\n");
		return 2;
	}

	freeaddrinfo(res);

	//ask for user input
	char action[4],filename[100];
	printf("Please enter the file name in the format : ftp <file>\n");
	err = scanf("%s %s", action,filename);
	if( err != 2 | strcmp(action,"ftp") != 0){
		fprintf(stderr, "wrong format: ftp <file>\n");
		exit(1);
	} 

#ifdef TIMER
	char dest_addr[100];
	printf("send to: %s\n", inet_ntop(checkinfo->ai_family,
		&(((struct sockaddr_in*)checkinfo->ai_addr)->sin_addr),
		dest_addr, 100)); 
	clock_t time;
#endif

	//check file exists
	if(!access(filename, F_OK)){
#ifdef TIMER
	time = clock();	
#endif
		err = sendto(sockfd, action, sizeof(action), 0,
			checkinfo->ai_addr, checkinfo->ai_addrlen);

		if(err==-1){
			perror("sendto1");
			return 1;
		}
	}else{
		fprintf(stderr,"%s: does not exist\n", filename);
		exit(0);
	}

	//receive message from server
	char ack[10];
	struct sockaddr src_addr;
	socklen_t addrlen = sizeof(src_addr);
	if(recvfrom(sockfd,ack, sizeof(ack), 0,
		 &src_addr, &addrlen) < 0){
		perror("recvfrom");
		exit(1);
	}
	
#ifdef TIMER
	time = clock() - time;
	printf("It takes %d clock ticks\n", (int)time);
	double mseconds = ((double)time*1000)/CLOCKS_PER_SEC;
	printf("This round trip takes: %.3f ms\n",mseconds*1000);
#endif

	if(strcmp(ack,"yes") != 0){
		fprintf(stderr, "acknowledge fails\n");
		exit(1);
	}
	printf("A file transfer can start\n");

	//open the file
	FILE * file = fopen(filename, "r");
	if(!file) perror("cannot open the file");

	//get the file size in bytes
	fseek(file, 0L, SEEK_END);
	long int file_size = ftell(file);
	fseek(file, 0L, SEEK_SET); //seek backk

	//total packet number
	int total_frag = ceil(((double)file_size)/MAXDATALEN);
	int last_packet_size = file_size%MAXDATALEN;

	//initialize packet info
	char dummy[200]; //used to count packet header bytes
	bzero(dummy, sizeof(char)*200);
	int frag_no = 1; //sequence number, starting from 1
	int data_size;
	if(total_frag > 1) data_size = MAXDATALEN;
	else data_size = last_packet_size;

	//create the packet to be sent
	int offset_header = sprintf(dummy, "%d:%d:%d:%s:", total_frag,
				frag_no, data_size, filename);
	int max_header = offset_header;
	int buffer_size = max_header + MAXDATALEN;
	char* buffer = malloc(sizeof(char)*buffer_size);
	bzero(buffer, buffer_size);
	//pump the header lines
	sprintf(buffer, "%d:%d:%d:%s:", total_frag,
			frag_no, data_size, filename);

        //for debugging
	int total_bytes;


	//set timeout to the following recvfrom call.
	//in case ACK/NACK is lost in transaction.
	//a round trip time is about 200ms, so I set timeout
	//to be 5*RRT.
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 400000;
	err = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
		&timeout, sizeof(timeout));
			
	//read the file
	while(!feof(file)){
		//read data from file
		int bytes = fread((buffer + offset_header), sizeof(char), MAXDATALEN, file);
		total_bytes += bytes;

		//finanlly send the packet
		int len = buffer_size*sizeof(char);
		err = sendto(sockfd, buffer, len, 0,
			checkinfo->ai_addr, checkinfo->ai_addrlen);
		if(err==-1){
			perror("sendto2");
			return 1;
		}

#ifdef DEBUG
//printf("packet number: %d, total %d\n", frag_no, total_frag);
#endif

		//wait for ACK/NACK from server
		int done = 1;
		struct sockaddr new_source;
		socklen_t addrlen = sizeof(new_source);
		char ack[5] = {'\0'};
		err = recvfrom(sockfd, ack, 5, 0, &new_source, &addrlen);
		int errsv = errno;
		if(err <0){
			if(errsv == EAGAIN)//timeout has occured
			{
				printf("timeout: ACK/NACK\n");
				//resend the packet
				total_bytes -= bytes;
				//seek back the file position
				fseek(file, -bytes, SEEK_CUR);
				continue;
			}else{
				perror("recvfrom--waiting for ACK/NACK");
				exit(1);
			}
		}

check_ack:
		if(strcmp(ack, "NACK") == 0){
			//resend the current packet
			total_bytes -= bytes;
			//seek back the file position
			fseek(file, -bytes, SEEK_CUR);
			printf("NACK! Resending the last packet\n");
			continue;
		}
		else if(strcmp(ack, "ACK") !=0) goto check_ack;
		printf("ACK\n");



		/* update packet header and buffer size */
		frag_no += 1;
		if(frag_no == total_frag) data_size = last_packet_size;

		offset_header = sprintf(buffer, "%d:%d:%d:%s:", total_frag,
				frag_no, data_size, filename);
		//in case the size of header increases,
		//expand the buffer.
		//this may never be executed -- can be removed by giving
		//a large inital buffer size.
		if(offset_header > max_header)
		{
			#ifdef DEBUG
			printf("Buffer size is expanded from %d",buffer_size);
			#endif
			max_header = offset_header;
			free(buffer);
			buffer_size = max_header + MAXDATALEN;
			buffer = malloc(sizeof(char)*buffer_size);
			bzero(buffer, buffer_size);
			//pump the header lines
			sprintf(buffer, "%d:%d:%d:%s:", total_frag,
				frag_no, data_size, filename);

			#ifdef DEBUG
			printf("to %d\n",buffer_size);
			#endif
		}
	}

		printf("Report by client: original file size is: "
			"%ld\n%d are actually sent.\n", file_size, total_bytes);

	close(sockfd);
	fclose(file);
	free(buffer);

	return 0;
}
