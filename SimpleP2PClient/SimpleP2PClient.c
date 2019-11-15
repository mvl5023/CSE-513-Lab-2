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
	
// Servers assigned to Port range 5000-9999
// Clients assigned to port range 10000+ ??
#define PORT 5000	
#define BUFSIZE 1024
	
/*---------------------------------------------------------------
* Function: StartsWith
* Input: const char *a, const char *b
* Output: int (acting as a boolean)
* Description:
* Compares the buffer in a to b and the
* length of b.
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
* Core of the program, after connections are setup. This
* receives buffers from the peers to then check and find
* out if files need:
* list - list of keys on the dependency list of the server
* read - reads the value from the key entered
* write - write to the server dependency list to be replicated to others.
* leave - leaves or exits the connection from the server
*
----------------------------------------------------------------*/	
void send_recv(int i, int sockfd)
{
	//used for sending/receiving commands to the server
	char send_buf[BUFSIZE];
	char recv_buf[BUFSIZE];
	int nbyte_recvd;
	
	if (i == 0){
		fgets(send_buf, BUFSIZE, stdin);
		if (strcmp(send_buf , "help\n")==0){
			printf("----------------------------------------------\n");
			printf("Welcome to the help menu. Connected to server at port: %d \n", PORT);
			printf("Commands: \n");
			printf("list -lists the dependency list on the server\n");
			printf("read(key) -reads the value from the entered key\n");
			printf("write(key,value) -Asks the server for the file to transfer\n");
			printf("                  Key should be a unique 5 digit integer.\n");
			printf("leave -promptly disconnects from the server\n");
			printf("----------------------------------------------\n");
		}else if (strcmp(send_buf , "leave\n") == 0) {
			printf("Leaving Server...\n");
			exit(0);
		}else if(strcmp(send_buf , "list\n") == 0){
			//send list command to the server to interpret
			printf("Dependency List from server at port: %d \n", PORT);
			send(sockfd, send_buf,strlen(send_buf), 0);
		}else if(StartsWith(send_buf,"read(")){
			printf("Read started.\n");
			send(sockfd, send_buf,strlen(send_buf), 0);
		}else if(StartsWith(send_buf,"write(")){
			printf("Write sent.\n");
			send(sockfd, send_buf,strlen(send_buf), 0);
		}else
			send(sockfd, send_buf, strlen(send_buf), 0);
	}else {
		nbyte_recvd=0;
		nbyte_recvd = recv(sockfd, recv_buf, BUFSIZE, 0);
		recv_buf[nbyte_recvd] = ' ';
		printf("%s\n" , recv_buf);
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
	else {
		printf("Connected to %d\n", PORT);
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
	//connect_request(&sockfd, &server_addr);	
	
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(0, &master);
	FD_SET(sockfd, &master);
	fdmax = sockfd;
	printf("Type help and press enter for the list of possible commands\n");
	while(1){
		read_fds = master;
		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1){
			perror("select");
			exit(4);
		}
		
		for(i=0; i <= fdmax; i++ ) {
			if(FD_ISSET(i, &read_fds))
				send_recv(i, sockfd);
		}
	}
	printf("client-quited\n");
	close(sockfd);
	return 0;
}
