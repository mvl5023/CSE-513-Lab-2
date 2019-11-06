/*
 ============================================================================
 Name        : SimpleP2PServer.c
 Author      : Joseph E. Tomasko, Jr.
 Version     : 2.3.1
 Copyright   : Your copyright notice
 Description : The simple server and all the computing
 	 	 	   needed from commands received from the clients
 	 	 	   that are connected
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
//directory access
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
//for random delays
#include <time.h>

// Servers assigned to port range 5000-9999
// Clients assigned to port range 10000+
#define PORT 5000
//#define PORT 4950
#define BUFSIZE 1024
#define COLUMNS 4
#define ROWS 32

//32 clients to a server.  We won't be simulating that much. I want casuality working
//format for each row:
// {Dependency_Letter/Port/Client, Key, Value(String?!),Timestamp, Database ID}
//may need a separate list for key and value
int dependency_list[ROWS][COLUMNS] = { NULL };

//2d array for key and value
//key will match with row num so then you can grab the value
//messages can be 32 char long
char key_value[ROWS][32] = { NULL };

void Addon_Dependency_List(int client_port)
{
	char s1[10];
	char s2[10];

	sprintf(s1, "%d",PORT);
	sprintf(s2, "%d",client_port);

	strcat(s1,s2);
	int serverpclientp=atoi(s1); //format 5000XXXXX

	for(int i=0;i<ROWS;i++)
	{
		if(dependency_list[i][0] == NULL){
			dependency_list[i][0]=serverpclientp;
			//printf("row %d contents: %d\n", i, dependency_list[i][0]);
			//printf("row %d contents: %d\n", i+1, dependency_list[i+1][0]);
			break;
		}
	}
}

/*---------------------------------------------------------------
 * Function: sleepRandomTime
 * Input: none
 * Output: wait/delay time
 * Description:
 * 	Waits for 1 to 5 seconds
 ----------------------------------------------------------------*/
void sleepRandomTime(){
	srand(time(NULL));
	float duration = rand()%5+1;
	sleep(duration);
	//printf("duration: %f \n", duration);
}

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
 * Function: send_to_all
 * Input: int j, int i, int sockfd, int nbytes_recvd, char *recv_buf, fd_set *master
 * Output: None
 * Description:
 * 	Sends a message to all peers connected.  This is what I used
 * 	initially to check the multi-server connections
 ----------------------------------------------------------------*/
void send_to_all(int j, int i, int sockfd, int nbytes_recvd, char *recv_buf, fd_set *master)
{
	//error checking
	if (FD_ISSET(j, master)){
		if (j != sockfd && j != i) {
			if (send(j, recv_buf, nbytes_recvd, 0) == -1) {
				perror("send");
			}
		}
	}
	send(j,recv_buf,nbytes_recvd,0);
}

/*---------------------------------------------------------------
 * Function: ListingFiles
 * Input: int i
 * Output: None
 * Description:
 * 	Sends messages to requesting peer the files stored in FILES that
 * 	are also files that are located on some of the peers.  A decent
 * 	amount of string manipulation
 ----------------------------------------------------------------*/
void ListingDependencyList(int q){
	char rowinfo[256];
	char s1[256];
	char s2=' ';

	for(int i=0;i<ROWS;i++)
		{
			//if it's null, it's empty past that point
			if(dependency_list[i][0] == NULL){
				break;
			}
			for(int j=0;j<COLUMNS;j++){
				//send contents at location
				sprintf(s1, "%d",dependency_list[i][j]);
				strncat(s1,&s2,1);
				//concatenate until done with the row then send
				strcat(rowinfo,s1);
				//clear for next one
				memset(s1,0,sizeof(s1));
			}
			//send back to the client
			int length=strlen(rowinfo);
			send(q,rowinfo,length,0);
			//clear all buffers for next round
			memset(rowinfo,0,sizeof(rowinfo));
		}
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
 * 	Also, if a socket disconnects or is lost, S_IP_PORT.txt is
 * 	deleted.  Reason being because the Ports and socket numbers
 * 	differ as each peer joins or leaves.
 * 	TODO save old ports for those that have connected before?
 ----------------------------------------------------------------*/
void send_recv(int i, fd_set *master, int sockfd, int fdmax)
{
	int nbytes_recvd, j;
	char recv_buf[BUFSIZE];
	char send_buf[BUFSIZE];
	struct dirent *de;  // Pointer for directory entry
	char begin[2];
	//if someone disconnects
	if ((nbytes_recvd = recv(i, recv_buf, BUFSIZE, 0)) <= 0) {
		if (nbytes_recvd == 0) {
			printf("socket %d hung up\n", i);
			//int i to char conversion
			sprintf(begin, "%d",i);
			// opendir() returns a pointer of DIR type.
			DIR *dr = opendir(".");
			if (dr == NULL)  // opendir returns NULL if couldn't open directory
			{
				printf("Could not open current directory" );
			}
			//search project directory for a socket num beginning
			//since rejoining gives you a new Port and Socket number
			while ((de = readdir(dr)) != NULL) {
					//printf("%s\n", de->d_name);
					if(de->d_name[0]==begin[0]){
						remove(de->d_name);
					}
			}
			closedir(dr);
		}else {
			perror("recv");
		}
		close(i);
		FD_CLR(i, master);
	}else {
		printf("Client%d sent: %s", i,recv_buf);
		bzero(send_buf, sizeof(send_buf));
		//for some reason strcmp doesn't work 1st time now.
		if (StartsWith(recv_buf, "write(")) {
			//write to the dependency array
			//client write function

			//replicated_write
		}else if(StartsWith(recv_buf,"read(")) {
			//read request from clients connected
			//client read function

			//
		}else if(StartsWith(recv_buf,"list\n")) {
			//read request from clients connected
			ListingDependencyList(i);
		}else if(StartsWith(recv_buf,"replicated ")) {
			//from the other servers
			//validateFileAndLength(i,recv_buf);
		}
	}
	//sends to all peers.  could be useful to show what's shared?
	//for(j = 4; j <= fdmax; j++){
	//		send_to_all(j, i, sockfd, nbytes_recvd, recv_buf, master );
	//}
	bzero(recv_buf, sizeof(recv_buf));
}

/*---------------------------------------------------------------
 * Function: connection_accept
 * Input: fd_set *master, int *fdmax, int sockfd, struct sockaddr_in *client_addr
 * Output: None
 * Description:
 * 	Finds the Socket to connect to then establishes with accept.
 * 	We also create the S_IP_PORT.txt file here to reference later
 * 	when peers start sharing and requesting, etc.
 ----------------------------------------------------------------*/
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
		//create a dependency list.  Send port number since IP is the same for all.
		Addon_Dependency_List(ntohs(client_addr->sin_port));
		//snprintf(newfile, sizeof(newfile), "%d_%s_%d.txt", newsockfd,inet_ntoa(client_addr->sin_addr),ntohs(client_addr->sin_port));
	}
}

/*---------------------------------------------------------------
 * Function: connect_request
 * Input: int *sockfd, struct sockaddr_in *my_addr
 * Output: None
 * Description:
 * 	Connection request.  Sets up the socket family, port, address.
 * 	The server initializes at 4950 everytime.
 ----------------------------------------------------------------*/
void connect_request(int *sockfd, struct sockaddr_in *my_addr)
{
	int yes = 1;
	//mike stuff
	int portNew = PORT;
	if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket");
		exit(1);
	}

	my_addr->sin_family = AF_INET;
	my_addr->sin_port = htons(PORT);
	my_addr->sin_addr.s_addr = INADDR_ANY;
	memset(my_addr->sin_zero, '.', sizeof my_addr->sin_zero);

	if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	if (bind(*sockfd, (struct sockaddr *)my_addr, sizeof(struct sockaddr)) == -1) {
		perror("Unable to bind");
		exit(1);
	}
	/*
	// Server binds to first free port >=5000
	int bindNum = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr));
	printf("Bind return value = %d \n", bindNum);
	if (bindNum < 0)
	{
		while((bindNum < 0) && (portNew < 9999))
		{
			printf("Unable to bind to port %d\n", portNew);
			portNew = portNew + 1;
			my_addr->sin_port = htons(portNew);
			printf("Attempting to bind to port %d \n", portNew);
			bindNum = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr));
			printf("Bind return value = %d \n", bindNum);
		}
	}
	if (bindNum < 0)
		printf("Failed to find free port - maximum number of servers reached.\n");
	else
		printf("Successfully bound on port %d\n", portNew);
	*/
	if (listen(*sockfd, 10) == -1) {
		perror("listen");
		exit(1);
	}
	printf("\nTCPServer Waiting for client on port %d\n", PORT);
	fflush(stdout);
}

/*---------------------------------------------------------------
 * Function: main
 * Input: void
 * Output: int
 * Description:
 * 	Here selects what peers are communicating with the server.
 * 	Sitting and listening once initialized.  If a new socket,
 * 	go to connection_accept, else sending or receiving
 ----------------------------------------------------------------*/
int main(void) {
	fd_set master;
	fd_set read_fds;
	int fdmax, i;
	int sockfd= 0;
	struct sockaddr_in my_addr, client_addr;

	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	connect_request(&sockfd, &my_addr);
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
	return EXIT_SUCCESS;
}

