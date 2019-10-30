#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>

// Servers assigned to port range 5000-9999
// Clients assigned to port range 10000+
#define PORT 5000
#define BUFSIZE 1024

long int findSize(char file_name[]) 
{ 
    // opening the file in read mode 
    FILE* fp = fopen(file_name, "r"); 
  
    // checking if the file exist or not 
    if (fp == NULL) { 
        printf("File Not Found!\n"); 
        return -1; 
    } 
  
    fseek(fp, 0L, SEEK_END); 
  
    // calculating the size of the file 
    long int res = ftell(fp); 
  
    // closing the file 
    fclose(fp); 
  
    return res; 
} 

void send_to_all(int j, int i, int sockfd, int nbytes_recvd, char *recv_buf, fd_set *master)
{

	if (FD_ISSET(j, master)){
		if (j != sockfd && j != i) {
			if (send(j, recv_buf, nbytes_recvd, 0) == -1) {
				perror("send");
			}
		}
	}
	//if (recv_buf && !recv_buf[0]) {
  	//	printf("recv_buf is empty\n");
	//}
	recv_buf[0]='A';
	recv_buf[1]='C';
	recv_buf[2]='K';
	recv_buf[3]='\n';
	send(i,recv_buf,nbytes_recvd,0);
	bzero(recv_buf, sizeof(recv_buf));
}

void send_recv(int i, fd_set *master, int sockfd, int fdmax)
{
	int nbytes_recvd, j;
	char recv_buf[BUFSIZE];// buf[BUFSIZE];
	char send_buf[BUFSIZE];

	//if someone disconnects
	if ((nbytes_recvd = recv(i, recv_buf, BUFSIZE, 0)) <= 0) {
		if (nbytes_recvd == 0) {
			printf("socket %d hung up\n", i);
		}else {
			perror("recv");
		}
		close(i);
		FD_CLR(i, master);
	}else { 
		printf("Client sent: %s", recv_buf);
		bzero(send_buf, sizeof(send_buf));
		if (strcmp(recv_buf , "filelist\n") == 0) {
			//send(sockfd, send_buf,sizeof(send_buf));
			//send(i , send_buf , strlen(send_buf) , 0);

		}
	}	
	bzero(recv_buf, sizeof(recv_buf)); 
	//sends to all peers.  could be useful to show what's shared?
	//	for(j = 0; j <= fdmax; j++){
	//		send_to_all(j, i, sockfd, nbytes_recvd, recv_buf, master );
	//	}
}
		
void connection_accept(fd_set *master, int *fdmax, int sockfd, struct sockaddr_in *client_addr)
{
	socklen_t addrlen;
	int newsockfd;
	
	addrlen = sizeof(struct sockaddr_in);
	if((newsockfd = accept(sockfd, (struct sockaddr *)client_addr, &addrlen)) == -1) {
		perror("accept");
		exit(1);
	}else {
		FD_SET(newsockfd, master);
		if(newsockfd > *fdmax){
			*fdmax = newsockfd;
		}
		printf("new connection from %s on port %d \n",inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
	}
}
	
void connect_request(int &sockfd, struct sockaddr_in &my_addr)
{
	int yes = 1;
	int portNew = PORT;
		
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket");
		exit(1);
	}
		
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(PORT);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	memset(my_addr.sin_zero, '.', sizeof my_addr.sin_zero);
		
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}
	
	// Server binds to first free port >=5000
	int bindNum = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr));
	printf("Bind return value = %d \n", bindNum);
	if (bindNum < 0) 
	{ 
		while((bindNum < 0) && (portNew < 9999))
		{
			printf("Unable to bind to port %d\n", portNew);
			portNew = portNew + 1;
			my_addr.sin_port = htons(portNew);
			printf("Attempting to bind to port %d \n", portNew);
			bindNum = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr));
			printf("Bind return value = %d \n", bindNum);
		}
	}
	if (bindNum < 0)
		printf("Failed to find free port - maximum number of servers reached.\n");
	else
		printf("Successfully bound on port %d\n", portNew);
	
	if (listen(sockfd, 10) == -1) {
		perror("listen");
		exit(1);
	}
	fflush(stdout);
}
int main()
{
	fd_set master;
	fd_set read_fds;
	int fdmax, i;
	int sockfd= 0;
	struct sockaddr_in my_addr, client_addr;
	
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	connect_request(sockfd, my_addr);
	FD_SET(sockfd, &master);
	
	fdmax = sockfd;
	while(1){
		read_fds = master;
		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1){
			perror("select");
			exit(4);
		}
		
		for (i = 0; i <= fdmax; i++){
			if (FD_ISSET(i, &read_fds)){
				if (i == sockfd)
					connection_accept(&master, &fdmax, sockfd, &client_addr);
				else
					send_recv(i, &master, sockfd, fdmax);
			}
		}
	}
	return 0;
}
