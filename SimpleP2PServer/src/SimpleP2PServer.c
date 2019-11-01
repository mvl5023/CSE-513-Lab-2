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

// Servers assigned to port range 5000-9999
// Clients assigned to port range 10000+
#define PORT 5000
//#define PORT 4950
#define BUFSIZE 1024
#define COLUMNS 4
#define ROWS 32

//32 clients to a server.  We won't be simulating that much. I want casuality working
//format for each row:
// {Dependency_Letter/Port/Client, Key, Timestamp, Database ID}
int dependency_list[ROWS][COLUMNS] = { NULL };


void Addon_Dependency_List(int client_port)
{
	int serverpclientp=(PORT*100000000)+client_port; //format 5000XXXXX
	for(int i=0;i<ROWS;i++)
	{
		if(dependency_list[i][0] == NULL){
			dependency_list[i][0]=serverpclientp;
			printf("row %d contents: %d\n", i, dependency_list[i][0]);
			printf("row %d contents: %d", i+1, dependency_list[i+1][0]);
			break;
		}
	}
}

/*---------------------------------------------------------------
 * Function: findSize
 * Input: char filename[]
 * Output: long int (file size in bytes)
 * Description:
 * 	Looks at the file in the FILES directory (files hosted on
 * 	the server and various "sharing" peers) and determines the
 * 	length of the file.
 ----------------------------------------------------------------*/
long int findSize(char file_name[])
{
    // opening the file in read mode
    FILE* fp = fopen(file_name, "r");

    // checking if the file exist or not
    if (fp == NULL) {
        printf("File Not Found!\n");
        return -1;
    }
    //look at the end
    fseek(fp, 0L, SEEK_END);
    // calculating the size of the file
    long int res = ftell(fp);
    // closing the file
    fclose(fp);

    return res;
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
 * Function: checkAvailableEndpoints
 * Input: int i (socket id), char *recv_buf
 * Output: None
 * Description:
 * 	scan each I_IP_PORT.txt file for the input filename,
 * 	if it's a match, display file name (bc it will have IP and Port
 * 	in it, then all the chunks in the file you as the requestor
 * 	can request
 ----------------------------------------------------------------*/
void checkAvailableEndpoints(int i, char *recv_buf){

	//is the request valid? scan each file, if the line isn't there
	//-failure
	struct dirent *de;  // Pointer for directory entry
	char begin[3];
	uint16_t endpoint_count=0;

	//for file stuff
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;

	//get the file name from receive buf, put into some char array
	char filename[256];
	char request_info[35] = "Format=S_IPADDR_PORT.txt\nEndpoint: ";
	char delim[]=" ";
	int string_piece=0;

    char *ptr = strtok(recv_buf,delim);

	while(ptr != NULL){
		if(string_piece==1){
			strcpy(filename,ptr);
		}
		//printf("%s\n",ptr);
		ptr=strtok(NULL,delim);
		string_piece++;
	}
	//filename will be able to be searched in the files on the server

	//begin will contain the socket number for comparison later
	sprintf(begin, "%d",i);
	// opendir() returns a pointer of DIR type.
	DIR *dr = opendir(".");
	if (dr == NULL)  // opendir returns NULL if couldn't open directory
	{
		printf("Could not open current directory" );
	}
    send(i,request_info,strlen(request_info),0);
    //search project directory for a socket num beginning
	//since rejoining gives you a new Port and Socket number
	//TODO modify for more peers
	while ((de = readdir(dr)) != NULL) {
			//TODO these checks are to specific and limiting, but
			//I want to see the information get back to the peer
			//requesting endpoints
			if(de->d_name[0]==begin[0]){
				//do nothing (because this is the same peer
			}else if (de->d_name[1]=='_'){
				//read file line by line
				  fp = fopen(de->d_name, "r");
				  if (fp == NULL)
				      exit(EXIT_FAILURE);
				  while ((read = getline(&line, &len, fp)) != -1) {
				      //printf("Retrieved line of length %zu:\n", read);
				      //printf("%s", line);
				      //if the line string is equal to FILENAME
				      //Tell the peer the file format
				      //tell them the IP and PORT (from file name)
					  if(strcmp(line,filename)==0){
					  //send Endpoints:
				      //file name with socket IP PORT.txt
				      send(i,de->d_name,strlen(de->d_name),0);
				      //increase endpoint count
				      endpoint_count++;
					  }

				  }
				  fclose(fp);
				  if (line)
					  free(line);
			}

	}
	//display endpoint count!
	closedir(dr);
	bzero(request_info,sizeof(request_info));
	if(endpoint_count!=0){
		sprintf(request_info,"\nTotal endpoints: %u\n",(unsigned int)endpoint_count);
		send(i,request_info,strlen(request_info),0);
	}else{
		sprintf(request_info,"No endpoints...");
		send(i,request_info,strlen(request_info),0);
	}
    bzero(request_info,sizeof(request_info));
	memset(filename,0,strlen(filename));
	memset(request_info,0,strlen(request_info));
	//printf("placeholder");
}

/*---------------------------------------------------------------
 * Function: WriteFileShared
 * Input: int i,char *filename, int size
 * Output: None
 * Description:
 * 	Initialize registry filename, a file pointer, and
 * 	char array.  Calls findEndpointFile to get the right
 * 	socket file to write to, then opens that file,
 * 	writes the shared file and the size to S_IP_PORT.txt
 * 	file.  So it can be scanned for later for request FILE
 ----------------------------------------------------------------*/
void WriteFileShared(int i,char *filename, int size){
	//get IP and port from
	//titled reg file
	char regfilename[256];
	FILE *file_pointer;
	char str[64];

	struct dirent *de;  // Pointer for directory entry
	char begin[2];
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
			//TODO what about higher socket numbers?
			if(de->d_name[0]==begin[0]){
				//get the right txt file name
				strcpy(regfilename,de->d_name);
			}
	}
	closedir(dr);

	file_pointer=fopen(regfilename,"a");
	if(file_pointer == NULL)
	{
		printf("error opening file!\n");
		goto fileclose;
	}
	//name of file in first line
	fwrite(filename, sizeof(char),sizeof(filename)+1,file_pointer);
	fwrite("\n",sizeof(char),1,file_pointer);
	//put the size too on next line
	sprintf(str, "%d",size);
	fwrite(str, sizeof(char),sizeof(str),file_pointer);
	fwrite("\n",sizeof(char),1,file_pointer);

fileclose:
	fclose(file_pointer);
	memset(str,0,strlen(str));
	memset(regfilename,0,strlen(regfilename));
	//time to chunk on peer
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
void ListingFiles(int i){
	//dirent.h stuff
	DIR *d;
	struct dirent *dir;
	d=opendir("./FILES");
	char path[256];
	char msg[16];
	uint16_t file_count;
	file_count = 0x0;
	//res is length
    long int res;

	if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
			if (dir->d_name[0] != '.'){
			int length = strlen(dir->d_name);
			send(i,dir->d_name,length,0);
			//findsize
			//put file directory on there, then concatenate the filename
			strcpy(path,"./FILES/");
			strcat(path,dir->d_name);
			res=findSize(path);
			if (res != -1){
			        sprintf(path,"-File size: %ld bytes \n", res);
			        send(i,path,strlen(path),0);
			}
    		file_count++;
    		//clear array
    		memset(path,0,strlen(path));
			}
			//bzero(dir->d_name, sizeof(dir->d_name));
        }
        closedir(d);
    }
	//note to self. Don't ever clear dir->d_name)!
	//TODO file count could be more but would need modified
	memset(msg,0,strlen(msg));
	sprintf(msg,"File Count: %u\n",(unsigned int)file_count);
	send(i,msg,strlen(msg),0);
}

/*---------------------------------------------------------------
 * Function: validateFileAndLength
 * Input: int i, char *recv_buf
 * Output: None
 * Description:
 * 	Similar to ListingFiles, but needs to check if these "share"
 * 	commands are valid and are on the server FILE list.  If it is
 * 	it will send the validated message back to the peer, then
 * 	write to a S_IP_PORT.txt file showing that this peer is sharing
 * 	the peer-entered file.
 ----------------------------------------------------------------*/
void validateFileAndLength(int i, char *recv_buf){
	//can only validate files on the server quickly this way
	//add the socket to the registry file
	DIR *d;
	struct dirent *dir;
	d=opendir("./FILES");
	char path[256];
	char filename[256];
	char filesize[16];
	int string_piece=0;
	char delim[]=" ";
	//filesize is length
    long int res;

    char *ptr = strtok(recv_buf,delim);

	while(ptr != NULL){
		if(string_piece==1){
			strcpy(filename,ptr);
		}else if(string_piece==2){
			strcpy(filesize,ptr);
		}
		//printf("%s\n",ptr);
		ptr=strtok(NULL,delim);
		string_piece++;
	}
	//convert string to long int for res
	char *addr;
	long int ret;
	ret = strtol(filesize, &addr,10);

	memset(path,0,strlen(path));

	if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
			if (dir->d_name[0] != '.'){
				//compare filename
				if(strcmp(filename,dir->d_name)==0){
				strcpy(path,"./FILES/");
				strcat(path,dir->d_name);
				res=findSize(path);
					if (res != -1){
						//is the size right too?
						if(ret==res){
							sprintf(path,"File Registration Success\n",res);
					        send(i,path,strlen(path),0);
					        //on success, keep in S_IP_PORT.txt
					        WriteFileShared(i,dir->d_name, res);
					        //TODO send chunking request
					        //edit file again with all available chunks on the working peer
						}else{
							sprintf(path,"File failure. Mismatch bytes.",res);
							send(i,path,strlen(path),0);
						}
					}
				}
			}
        }
        closedir(d);
		memset(filename,0,strlen(filename));
		memset(filesize,0,strlen(filesize));
		//if nothing happened...failure.
		if(path[0]=='\0'){
			sprintf(path,"Failure. File not on server\n",res);
			send(i,path,strlen(path),0);
    	}
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
			ListingFiles(i);
		}else if(StartsWith(recv_buf,"read(")) {
			//read request from clients connected
			checkAvailableEndpoints(i,recv_buf);
		}else if(StartsWith(recv_buf,"replicated ")) {
			//from the other servers
			validateFileAndLength(i,recv_buf);
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

