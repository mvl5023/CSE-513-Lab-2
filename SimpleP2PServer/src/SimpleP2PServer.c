/*
 ============================================================================
 Name        : SimpleP2PServer.c
 Author      : Joseph E. Tomasko, Jr. & Michael Lipski
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
#include <string>
#include <vector>
//for random delays
#include <time.h>

// Servers assigned to port range 5000-9999
// Clients assigned to port range 10000+
#define PORT 5000
//#define PORT 4950
#define BUFSIZE 1024
#define COLUMNS 4
#define ROWS 32
#define NSERVS 50

//32 clients to a server.  We won't be simulating that much. I want casuality working
//format for each row:
// {Dependency_Letter/Port/Client, Key, Value(String?!),Timestamp, Database ID}
//may need a separate list for key and value
int dependency_list[ROWS][COLUMNS] = { NULL };

//2d array for key and value
//key will match with row num so then you can grab the value
//messages can be 32 char long
char key_value[ROWS][32]={NULL};

int myport;					// fixed port assignment for server, receiving
int port_hi = PORT;			// largest value fixed server port
int num_servers = 0;		// number of servers active
int local_time=0;

// Custom struct for server history
struct record_entry {
	int client;
	int server;
	int key;
	int timestamp;
	char value[32];
};

// Custom struct for queued replicated_writes
struct queue_entry {
	record_entry rwrite;
	std::vector<record_entry> deps;
};

// Tracking history of server operations
std::vector<record_entry> server_record;

// Repository of queued replicated_writes
std::vector<queue_entry> queued_writes;

void updateLocalTime(){
	local_time++;
}

void clientWritingtoDependency(int client_port, char *recv_buf){
	//attach current dependency_CLIENT
	// 1. to replicated write
	//		sending between servers needs developed

	//2. Then update dependency_CLIENT list
	//parse recv_buf for KEY
	int client_key_int=0;
	char client_key[5];
	record_entry newentry;		// updating server record

	for(int i=0;i<5;i++){
		client_key[i]=recv_buf[i+6];
	}
	client_key_int=atoi(client_key);
	//make sure client key is an int
	if(client_key_int!=0){
		//find matching row
		char s1[10];
		char s2[10];
		sprintf(s1, "%d",myport);
		sprintf(s2, "%d",client_port);
		strcat(s1,s2);
		int row_identifer=atoi(s1);
		for(int i=0;i<ROWS;i++)
		{
			if(dependency_list[i][0] == row_identifer){
				//add to the list the KEY, with Timestamp and DB_ID
				dependency_list[i][1]=client_key_int;
				// 3. update local server time
				//updateLocalTime();
				dependency_list[i][2]=local_time;
				//DB ID can be the server PORT since it will differ from server to server
				dependency_list[i][3]=myport;
				break; //break for loop
			}
		}

		//here we have to add the string value from recv buf to the string list
		//FORMAT: "5000XXXXX string"
		for(int q=0;q<ROWS;q++){
			if(key_value[q][0]==NULL){
				//init the string with KEY to the beginning
				//grab the actual string to store
				for(int w=0;w<32;w++){
					if(recv_buf[w+6]!=')'){
						key_value[q][w]=recv_buf[w+6];
					}else
						goto end;
				}

			}else if(key_value[q][0]==recv_buf[6] && key_value[q][1]==recv_buf[7]
			 && key_value[q][2]==recv_buf[8] && key_value[q][3]==recv_buf[9] && key_value[q][4]==recv_buf[10]){
				//if it matches, clear the slot with the new message
				memset(key_value[q],0,strlen(key_value[q]));
				//new message
				for(int w=0;w<32;w++){
					if(recv_buf[w+6]!=')'){
						key_value[q][w]=recv_buf[w+6];
					}else
						goto end;
				}
			}
		}//end for
		

		
	}//end if key
end:
	// create new server record entry
	newentry.client = client_port;
	newentry.server = myport;
	newentry.key = client_key_int;
	newentry.timestamp = local_time;
	for (int jj = 0; jj < 256; jj++) {
		if(recv_buf[jj+13]!=')'){
			newentry.value[jj] = recv_buf[jj+13];
		} else {
			break;
		}
	}
	
	// update server record
	server_record.insert(server_record.begin(), newentry);
	
	printf("Server record updated:\n");
	printf("New entry client = %d\n", newentry.client);
	printf("New entry key = %d\n", newentry.key);
	printf("New entry timestamp = %d\n", newentry.timestamp);
	printf("New entry value = %s\n", newentry.value);
	
	printf("Done with write.\n");
}

/*---------------------------------------------------------------
 * Function: dependency_check
 * Input: char recv_buf[BUFSIZE]
 * Output: Nothing
 * Description:
 * Upon receipt of replicated_write from another server, do the
 * following:
 * -Process socket buffer to extract write plus dependency list; 
 * -Perform dependency check on replicated_write;
 * -Recheck queued messages for dependency resolution.
 ----------------------------------------------------------------*/
void dependency_check(char *recv_buf){
	record_entry rep_write;
	std::vector<record_entry> depends;
	int is_met[depends.size()];
	int sums = 0;
	
	//process socket buffer
	
	
	printf("Received replicated_write from server %d\n", rep_write.server);
	
	//dependency check on replicated write
	printf("Checking dependencies...\n");
	for (int i = 0; i < depends.size(); i++) {		// outer loop: each element in rep_write dependency
		is_met[i] = 0;
		for (int j = 0; j < server_record.size(); j++){			// inner loop: check against server history
			if ((depends[i].key == server_record[j].key) && (depends[i].server == server_record[j].server)) {
				if (depends[i].timestamp == server_record[j].timestamp) {
					is_met[i] = 1;
					break;
				}
			}
		}
		sums += is_met[i];
		if (is_met[i] == 0) {		// dependency not met ...
			queue_entry queue_add;
			queue_add.rwrite = rep_write;
			queue_add.deps = depends;
			queued_writes.push_back(queue_add);		// so queue replicated_write
			printf("Dependencies not met, queueing replicated_write.\n");
			break;
		}
	}
	
	// if dependencies are met, commit replicated_write to server history
	if (sums == depends.size()) {
		server_record.insert(server_record.begin(), rep_write);
		printf("Dependencies met, committing replicated write.\n");
		printf("Key = %d\n", rep_write.key);
		printf("Value = %s\n", rep_write.value);
		printf("Timestamp = %d\n", rep_write.timestamp);
		updateLocalTime();
	}
	
	// check queued writes for newly met dependencies
	
	printf("Checking queued writes...\n");
	int rerun = 0;
	while (rerun == 0) {
		rerun = 0;
		for (int i = 0; i < queued_writes.size(); i++) {		// outer loop: each queued write
			queue_entry candidate = queued_writes[i];
			sums = 0;
			for (int j = 0; j < candidate.deps.size(); j++){			// inner loop: each dependency of a queued write
				is_met[j] = 0;
				for (int k = 0; k < server_record.size(); k++) {
					if ((candidate.deps[j].key == server_record[k].key) && (candidate.deps[j].server == server_record[k].server)) {
						if (candidate.deps[j].timestamp == server_record[k].timestamp) {
							is_met[j] = 1;
							break;
						}
					}
				}
				sums += is_met[j];
			}
			// all dependencies of queued write are met
			if (sums == candidate.deps.size()) {
				server_record.insert(server_record.begin(), queued_writes[i].rwrite);		// process queued write
				printf("Queued write dependencies met; committing to server record.\n");
				printf("Key = %d", queued_writes[i].rwrite.key);
				printf("Value = %s", queued_writes[i].rwrite.value);
				printf("Timestamp = %d", queued_writes[i].rwrite.timestamp);

				queued_writes.erase(queued_writes.begin() + i);				//remove replicated_write from queue
				rerun = 1;
			}
		}
	}

}



/*---------------------------------------------------------------
 * Function: Addon_Dependency_List
 * Input: int client_port
 * Output: Nothing
 * Description:
 * Once a client joins, adds port of server and port of client to the global
 * dependency list of the server
 ----------------------------------------------------------------*/
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
 * Function: ListingDependencyList
 * Input: int q
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
			strcat(rowinfo,"\n");
			//send back to the client
			int length=strlen(rowinfo);
			send(q,rowinfo,length,0);
			//clear all buffers for next round
			memset(rowinfo,0,sizeof(rowinfo));
		}
}

void readingTheKey(int client_port, int q, char *recv_buf){
	//here just send the message back to the client
	char send_msg[256];
	char receiving[6]={NULL};
	char key_str[6]={NULL};

	int read_msg_time=0;
	int server_id=0;
	
	record_entry newentry;		// updating server record



	for(int i=0;i<ROWS;i++){
		//get the key!
		for(int j=0;j<5;j++){
				key_str[j]=key_value[i][j];
				receiving[j]=recv_buf[j+5];
		}

		if(strcmp(receiving,key_str)==0){
			//if the key matches, send to the client asking for it
			strcpy(send_msg,key_value[i]);
			send(q,send_msg,strlen(send_msg),0);
			break;
		} else if(key_value[i][0]==NULL){
			//if empty, end of message list
			sprintf(send_msg,"Not a valid key\n");
			send(q,send_msg,strlen(send_msg),0);
			break;
		}
	}

	//then add to the dependency list after reading.
	int client_key_int=atoi(key_str);
	//scan dependency_list for first key match then get the time from there
	for(int i=0;i<ROWS;i++){
		if(dependency_list[i][1]==client_key_int){
			read_msg_time=dependency_list[i][2];
			server_id=dependency_list[i][3];
		}
	}

	//make sure client key is an int
	for(int i=0;i<ROWS;i++){
		if(dependency_list[i][0] == NULL){
			if(client_key_int!=0){
						//find matching row
				char s1[10];
				char s2[10];
				sprintf(s1, "%d",myport);
				sprintf(s2, "%d",client_port);
				strcat(s1,s2);
				int row_identifer=atoi(s1);
				//TODO this client_port is incorrect.
				dependency_list[i][0]=row_identifer;
				//add to the list the KEY, with Timestamp and DB_ID
				dependency_list[i][1]=client_key_int;
				//here it won't be local_time.  It'll be the message key's time
				dependency_list[i][2]=read_msg_time;
				//DB ID can be the server PORT since it will differ from server to server
				dependency_list[i][3]=server_id; //may come from a different server
				break; //break for loop
			}
		}
	}
	memset(send_msg,0,strlen(send_msg));
	memset(receiving,0,strlen(receiving));
	memset(key_str,0,strlen(key_str));
	
	// create new server record entry
	newentry.client = client_port;
	newentry.server = myport;
	newentry.key = client_key_int;
	newentry.timestamp = local_time;
	
	int maxtime = 0;
	int maxindex = 0;
	int present = 0;
	for (int jj = 0; jj < server_record.size(); jj ++) {
		if (newentry.key == server_record[jj].key) {
			present = 1;
			if (server_record[jj].timestamp > maxtime) {
				maxtime = server_record[jj].timestamp;
				maxindex = jj;
			}
		}
	}
	if (present == 1) {
		for (int jj = 0; jj < strlen(server_record[maxindex].value); jj++){
			newentry.value[jj] = server_record[maxindex].value[jj];
		}
		// update server record
		server_record.insert(server_record.begin(), newentry);
		
		printf("Server record updated:\n");
		printf("New entry client = %d\n", newentry.client);
		printf("New entry key = %d\n", newentry.key);
		printf("New entry timestamp = %d\n", newentry.timestamp);
		printf("New entry value = %s\n", newentry.value);
		
		printf("Finished processing read.\n");
	}
	else {
		printf("Key not found.\n");
	}
}


/*---------------------------------------------------------------
 * Function: server2server
 * Input: char* send_buf
 * Output: None
 * Description:
 * 	Sends contents of send_buf to all other servers
 ----------------------------------------------------------------*/
void server2server(char *send_buf)
{
	int bindNum;
	int yes = 1;
	int resock[myport-PORT];
	struct sockaddr_in server_addr;
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	// Iterate through all data center port assignments (not including self port)
	for (int i = PORT; i < PORT+num_servers; i++)
	{
		if (i != myport){
			int j = i - PORT;
			if ((resock[j] = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
				perror("Socket");
				exit(1);
			}
			server_addr.sin_port = htons(i);
			sleepRandomTime();						// add random delay
			if(connect(resock[j], (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
				perror("connect");
			}
			else {
				send(resock[j], send_buf, BUFSIZE, 0);
				close(resock[j]);
			}
		}
	}
}

/*---------------------------------------------------------------
 * Function: send_recv
 * Input: int i, fd_set *master, int sockfd, int fdmax
 * Output: None
 * Description:
 ----------------------------------------------------------------*/
void send_recv(int i, fd_set *master, int sockfd, int fdmax, struct sockaddr_in *client_addr)
{
	int nbytes_recvd, j;
	int newport;
	char recv_buf[BUFSIZE];
	char send_buf[BUFSIZE];
	struct dirent *de;  // Pointer for directory entry
	char begin[2];
	int client_port=ntohs(client_addr->sin_port);
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
		printf("Client at port %d sent: %s\n", client_port,recv_buf);
		//getting the PORT to identify in other functions!
		bzero(send_buf, sizeof(send_buf));
		//for some reason strcmp doesn't work 1st time now.
		if (StartsWith(recv_buf, "write(")) {
			//write to the dependency array
			//client write function
			updateLocalTime();
			clientWritingtoDependency(client_port,recv_buf);
			
			// TODO : replicate write for other servers
			// -translate write + attached dependencies to string, store to send_buf
			
			
			// broadcast replicated_write to all other servers
			server2server(send_buf);
		}else if(StartsWith(recv_buf,"read(")) {
			//read request from clients connected
			//client read function
			readingTheKey(client_port,i,recv_buf);
			//
		}else if(StartsWith(recv_buf,"list\n")) {
			//read request from clients connected
			ListingDependencyList(i);
		}else if(StartsWith(recv_buf,"replicated_write(")) {
			//from the other servers
			
			//process replicated_writes and perform dependency checks
			//dependency_check(recv_buf);
			
		}
		else {
			newport = atoi(recv_buf);
			if (newport > port_hi)
				port_hi = newport;
				num_servers = port_hi - PORT + 1;
			printf("%d servers active.\n", num_servers);
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
	char recv_buf[BUFSIZE];


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
		if (recv(newsockfd, recv_buf, BUFSIZE, MSG_PEEK) <= 0) {
			//create a dependency list.  Send port number since IP is the same for all.
			Addon_Dependency_List(ntohs(client_addr->sin_port));
		}
		else {
			send_recv(newsockfd, master, sockfd, *fdmax, client_addr);
		}
		//snprintf(newfile, sizeof(newfile), "%d_%s_%d.txt", newsockfd,inet_ntoa(client_addr->sin_addr),ntohs(client_addr->sin_port));
	}
}

/*---------------------------------------------------------------
 * Function: connect_request
 * Input: int *sockfd, struct sockaddr_in *my_addr
 * Output: None
 * Description:
 * 	Connection request.  Sets up the socket family, port, address.
 * 	The server attempts to connect to port 5000 and increments
 * 	until it finds the first available port
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

	// Server binds to first free port >=5000
	int bindNum = bind(*sockfd, (struct sockaddr *)my_addr, sizeof(struct sockaddr));
	printf("Bind return value = %d \n", bindNum);
	if (bindNum < 0)
	{
		while((bindNum < 0) && (portNew < 9999))
		{
			printf("Unable to bind to port %d\n", portNew);
			portNew = portNew + 1;
			my_addr->sin_port = htons(portNew);
			printf("Attempting to bind to port %d \n", portNew);
			bindNum = bind(*sockfd, (struct sockaddr *)my_addr, sizeof(struct sockaddr));
		}
	}
	if (bindNum < 0)
		printf("Failed to find free port - maximum number of servers reached.\n");
	else
	{
		getsockname(*sockfd, (struct sockaddr *)&my_addr, (socklen_t *)sizeof(struct sockaddr));
		myport = ntohs(my_addr->sin_port);
		printf("Successfully bound on port %d\n", myport);
		num_servers = myport + 1 - PORT;
	}

	// Connect to other servers
	if (myport > 5000)
	{
		char msgp[10];
		sprintf(msgp, "%d", myport);
		int resock[myport-PORT];
		struct sockaddr_in server_addr;


		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
		for (int i = PORT; i < myport; i++)
		{
			int j = i - PORT;
			if ((resock[j] = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
				perror("Socket");
				exit(1);
			}
			server_addr.sin_port = htons(i);
			printf("bindNum= %d\n", bindNum);
			if(connect(resock[j], (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
				perror("connect");
			}
			else {
				send(resock[j], msgp, BUFSIZE, 0);
				close(resock[j]);
			}
		}
	}
	
	if (listen(*sockfd, 10) == -1) {
		perror("listen");
		exit(1);
	}
	printf("\nTCPServer Waiting for client on port %d\n", myport);
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
					send_recv(i, &master, sockfd, fdmax, &client_addr);
			}
		}
	}
	return EXIT_SUCCESS;
}
