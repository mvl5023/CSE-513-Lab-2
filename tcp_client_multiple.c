#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

// Servers assigned to port range 5000-9999
// Clients assigned to port range 10000+
#define PORT 5000
#define BUFSIZE 1024

/*---------------------------------------------------------------
 * Function: StartsWith
 * Input: const char *a, const char *b
 * Output: int (acting as a boolean)
 * Description:
 * 	Compares the buffer in a to b and the
 * 	length of b.
 * 	Example: a="share blahblahblah"
 * 			 b="share "
 * 	It will register that a does start with b. (result in 1)
 ----------------------------------------------------------------*/	
int StartsWith(const char *a, const char *b)
{
   if(strncmp(a, b, strlen(b)) == 0) return 1;
   return 0;
}

/*---------------------------------------------------------------
 * Function: send_recv
 * Input: int i, fd_set *master, int sockfd, int fdmax
 * Output: None
 * Description:
 * 	Core of the program, after connections are setup.  This
 * 	receives buffers from the peers to then check and find
 * 	out if files need:
 * 		Shared 		- enable sharing with other peers
 * 		Listed 		- list of shareable files
 * 		Requested 	- list of endpoints that are sharing specific
 * 					  files
 *		FileChunk	- download from another peer. 
 *		Leave		- leaves or exits the connection from the server
 *
 ----------------------------------------------------------------*/	
void send_recv(int i, int sockfd)
{
	//used for sending/receiving commands to the server
	char send_buf[BUFSIZE];
	char recv_buf[BUFSIZE];
	int nbyte_recvd;
	int filenum;
	filenum = 0;
begin:	
	if (i == 0){
		fgets(send_buf, BUFSIZE, stdin);
		if (strcmp(send_buf , "help\n")==0){
			printf("----------------------------------------------\n");
			printf("Welcome to the help menu.\n");
			printf("Here are the following commands: \n");
			printf("filelist          -lists the downloadable files on the server\n");
			printf("share #	  	  -shares the # amount of files(1-9) with server and/or peers\n");
			printf("request FILENAME  -Asks the server for the file to transfer\n");
			printf("filechunk SBBB	  -Request a filechunk from socket S. Include byte size to specify file\n"); 
			printf("leave             -promptly disconnects from the server\n");
		}else if (strcmp(send_buf , "leave\n") == 0) {
			printf("Leaving Server file system...\n");
			exit(0);
		}else if(strcmp(send_buf , "filelist\n") == 0){
			//send filelist command to the server to interpret
			printf("----------------------------------------------\n");
			send(sockfd, send_buf,strlen(send_buf), 0);
		}else if(StartsWith(send_buf,"share ")){
			//send share and filename listed. must go to the server in the correct format
			printf("----------------------------------------------\n");
			printf("Share initiated.\n");
			filenum = send_buf[6] - '0';
			if(filenum<0||filenum>9){
				printf("Too many files.  Exiting, type help for commands.\n");
				goto begin;
			}
			printf("You have %d to share\n",filenum);
			printf("NOTE: this must be in the client's directory.\n");
			printf("Please format to be the complete filename then a space, then the size in bytes\n");
			printf("Example: share dummy.txt 155\n");
			bzero(send_buf,sizeof(send_buf));
			for(int i=0; i<filenum;i++){
				fgets(send_buf, BUFSIZE, stdin);
				send(sockfd, send_buf,strlen(send_buf), 0);
			}

			printf("type help and press enter for the list of commands\n");
		}else if(StartsWith(send_buf,"request ")){
			printf("----------------------------------------------\n");
			printf("File request initiated.\n");
			send(sockfd, send_buf,strlen(send_buf), 0);
		}else if(StartsWith(send_buf , "chunk ")){
			printf("----------------------------------------------\n");
			printf("File Chunk Request initiated.\n");
			send(sockfd, send_buf,strlen(send_buf), 0);
		}else
			send(sockfd, send_buf, strlen(send_buf), 0);
	}else {
		nbyte_recvd=0;
		nbyte_recvd = recv(sockfd, recv_buf, BUFSIZE, 0);
		recv_buf[nbyte_recvd] = ' ';
		printf("%s\n" , recv_buf);
		/*printf("Chunk requested."  recv_buf);
		if(strcmp(recv_buf , "filechunk ") == 0){
			//the peer has a request for a chunk then
			printf("----------------------------------------------\n");
			send(sockfd, send_buf,strlen(send_buf), 0);
		}*/
		fflush(stdout);
		bzero(recv_buf, sizeof(recv_buf));
	}
}
		
void connect_request(int *sockfd, struct sockaddr_in *server_addr)
{
	if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket");
		exit(1);
	}
	server_addr->sin_family = AF_INET;
	server_addr->sin_port = htons(PORT);
	server_addr->sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(server_addr->sin_zero, '.', sizeof server_addr->sin_zero);
	
	if(connect(*sockfd, (struct sockaddr *)server_addr, sizeof(struct sockaddr)) == -1) {
		perror("connect");
		exit(1);
	}
}

/*---------------------------------------------------------------
 * Function: main
 * Input: 
 * Output: 
 * Description:
 * 	Sends a connect request to the server, then from there
 *	your connected.  Then go to send and receive.
 ----------------------------------------------------------------*/
int main()
{
	int sockfd, fdmax, i;
	struct sockaddr_in server_addr;
	fd_set master;
	fd_set read_fds;
	
	connect_request(&sockfd, &server_addr);
	FD_ZERO(&master);
        FD_ZERO(&read_fds);
        FD_SET(0, &master);
        FD_SET(sockfd, &master);
	fdmax = sockfd;
	printf("type help and press enter for the list of commands\n");
	while(1){
		read_fds = master;
		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1){
			perror("select");
			exit(4);
		}
		
		for(i=0; i <= fdmax; i++ )
			if(FD_ISSET(i, &read_fds))
				send_recv(i, sockfd);
	}
	printf("client-quited\n");
	close(sockfd);
	return 0;
}
