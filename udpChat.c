#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#define BUFSIZE 1024


pthread_mutex_t	mutex = PTHREAD_MUTEX_INITIALIZER;
pid_t mthread;

/* Reliable version of signal(), using POSIX sigaction().  */
void *signal_safe(int signo, void *func)
{
   struct sigaction    act, oact;
   act.sa_handler = func;
   sigemptyset(&act.sa_mask);
   act.sa_flags = 0;
   if (signo == SIGALRM) {
       act.sa_flags |= SA_INTERRUPT;
   } else {
       act.sa_flags |= SA_RESTART;
   }
   if (sigaction(signo, &act, &oact) < 0)
       return(SIG_ERR);
   return(oact.sa_handler);
}

void sigalrm(int signo)
{ }

int maxSz = 10;
int currSz = 0;

struct entry {
	char uname[25];
	struct sockaddr_in clntaddr;
	char status;
};

struct servthread_args {
    char *buf;
    struct sockaddr_in *clntaddr;
};

struct entry *TABLE;

int getACK(char *, int, struct sockaddr_in *, socklen_t *);
int findEntry(char *);
void udpServer(char *);
void udpClient(char *, char *, char *, char *);
void initTABLE(void);
void resizeTABLE(void);
void addEntry(char *, struct sockaddr_in *);
void broadcastTABLE(void);


void error(char *buf) {
    perror(buf);
    exit(1);
}

void die(char *buf) {
	fprintf(stderr, "%s\n", buf);
	exit(1);
}

void *serverRequest(void *ptr)
{
	int len;
	char *buf, *uname;
	struct sockaddr_in *clntaddr;
	struct servthread_args *args;

	args = (struct servthread_args *)ptr;	
	buf = args->buf;
	clntaddr = args->clntaddr;

	fprintf(stderr, "Now pthread has %s\n", buf);
	
	len = strlen(buf);

	// Can recieve three types of messages 
	// [INIT]
	// reg/de-reg
	// [STORE] messages

	if (strstr(buf, "[INIT]") == buf) {
		uname = strstr(buf, "]") + 1;
		addEntry(uname, args->clntaddr);
		broadcastTABLE();
	}	
	else if (strstr(buf, "[DEREG]") == buf) {
		uname = strstr(buf, "]") + 1;
		int pos = findEntry(uname);
		TABLE[pos].status = 0;
		broadcastTABLE();
	}
	else if (strstr(buf, "[REG]") == buf) {
		uname = strstr(buf, "]") + 1;
		int pos = findEntry(uname);
		TABLE[pos].status = 1;
		broadcastTABLE();
		//send stored messages
	}
	else if (strstr(buf, "[STORE]") == buf) {
		
	}
	else {
		fprintf(stderr, "Unidentified tag recieved\n");
	}

	free(args->buf);
	free(args->clntaddr);
	free(args);

	return NULL;	
}

int main(int argc, char **argv) {

	if (argc < 2)
		error("Insufficient args");
		
	signal_safe(SIGALRM, sigalrm);
    mthread = pthread_self();
    
    if (strcmp("-s", argv[1]) == 0) {
        udpServer(argv[2]);
    } else if (strcmp("-c", argv[1]) == 0) {
        if (argc == 6)
            udpClient(argv[2], argv[3], argv[4], argv[5]);
        else
            error("Incorrect command line arguments");
    } else
        error("Incorrect command line arguments");
	
	return 0;
}

void udpServer(char *port_str)
{
    int servSock, port, n;
    struct sockaddr_in servaddr;
    struct servthread_args *args;
    socklen_t clntlen;
	pthread_t tid;
	char ipstr[20];

    port = atoi(port_str);

	if (port < 1024 || port > 65535)
		error("invalid port number");

    if ((servSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        error("ERROR opening socket");

    memset((char *)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons((unsigned short)port);

    if (bind(servSock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 )
        error("ERROR binding");

	initTABLE();
	fprintf(stderr, "server up..LESSGOOO..GET THIS CHAT GOIIN\n");

    clntlen = sizeof(struct sockaddr_in);

    for ( ; ;) {
		args = malloc(sizeof(*args));
        args->clntaddr = malloc(sizeof(struct sockaddr_in));
        args->buf = malloc(BUFSIZE);
        n = recvfrom(servSock, args->buf, BUFSIZE, 0,
						(struct sockaddr *)args->clntaddr, &clntlen);
		
		if (errno == EINTR) {
			perror("I've been interrupted");
			continue;
		}

        if (n < 0 ) {
            perror("ERROR on recvfrom()");
            continue;
        }

        
		fprintf(stderr, "recieved %s from client with address %s :%u\n", args->buf, inet_ntop(AF_INET, &(args->clntaddr->sin_addr.s_addr), ipstr, 20), ntohs(args->clntaddr->sin_port)); 

        if (sendto(servSock, "ACK", 4, 0, (struct sockaddr *)args->clntaddr, clntlen) < 4){
			perror("sending ACK failed\n");
            continue;
        }

		fprintf(stderr, "ACK sent to client\n");
        
		//make sure buff is null terminated
		args->buf[BUFSIZE - 1] = '\0';
        pthread_create(&tid, NULL, serverRequest,args);
    } //end for(; ;)
}


void *usrRequest(void *ptr)
{
	int sock;
	struct sockaddr_in *servAddr;
	size_t cap;
	ssize_t sz;
	char buf[BUFSIZE], *input, ipstr[20];
	socklen_t clntlen;
	
	clntlen = sizeof(struct sockaddr_in);
	input = NULL;
	servAddr = (struct sockaddr_in *)ptr;
	
fprintf(stderr, "server address %s :%u\n", inet_ntop(AF_INET, &(servAddr->sin_addr.s_addr), ipstr, 20), ntohs(servAddr->sin_port)); 
			
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		error("socket() failed");
		
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500 * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
	
	while (1) {
		while ((sz = getline(&input, &cap, stdin)) > 0) {
			fprintf(stderr, "mama thread id is %ld", mthread);
			
			if (strstr(input, "reg ") == input) {
				input[sz - 1] = '\0';
				strcpy(buf, "[REG]");
				input[BUFSIZE - 1] = '\0';
				strncat(buf, input + 4, BUFSIZE - 5);
				sendto(sock, buf, strlen(buf) + 1, 0,  (struct sockaddr *)servAddr, clntlen);
				
				pthread_kill(mthread, SIGALRM);
				pthread_mutex_lock(&mutex);
				if (getACK(buf, sock, servAddr, &clntlen) == 0)
					die(">>> [Server not responding]\n>>> [Exiting]");
				pthread_mutex_unlock(&mutex);
			}
			else if (strstr(input, "dereg ") == input) {
				fprintf(stderr, "server address %s :%u\n", inet_ntop(AF_INET, &(servAddr->sin_addr.s_addr), ipstr, 20), ntohs(servAddr->sin_port)); 
				int i;
				char *uname = input + 6;
				uname[strlen(uname) - 1] = '\0';
			
				if (findEntry(uname) < 0) {
					fprintf(stderr, ">>> Invalid username, try again.\n>>> ");
					continue;	
				}
				
				strcpy(buf, "[DEREG]");
				strncat(buf, uname, BUFSIZE - 5);
				
		
				sendto(sock, buf, strlen(buf) + 1, 0, (struct sockaddr *)servAddr, clntlen);
				
				//pthread_kill(mthread, SIGALRM);
				
			//	pthread_mutex_lock(&mutex);
				
				fprintf(stderr, "4");
				if (getACK(buf, sock, servAddr, &clntlen) == 0)
					die(">>> [Server not responding]\n>>> [Exiting]");
			
			//	pthread_mutex_unlock(&mutex);
				fprintf(stderr, ">>>[You are offline. Bye.]\n>>> ");
			}
			
			
			else if (strstr(input, "send ") == input) {
		
			}
			else {
				fprintf(stderr, "ERROR: unknown command. Try again.\n");
			}
			
			fprintf(stdin, ">>> ");
		}
	}
	exit(1);
}

void udpClient(char *uname, char *servIP, char *servPort, char *clntPort)
{
	int servSock, mySock, n, i, sock;
	struct sockaddr_in *servAddr, myAddr, clntAddr;
	char buf[BUFSIZE], ipstr[20];
	socklen_t servlen, mylen;
	pthread_t tid;
	int val = 1;
	servlen = mylen =  sizeof(struct sockaddr_in);

	// server Address
	servAddr = malloc(sizeof(struct sockaddr_in));
	memset(servAddr, 0, sizeof(*servAddr));
	servAddr->sin_family = AF_INET;
	servAddr->sin_port = htons((unsigned short)atoi(servPort));
	if (inet_aton(servIP, &servAddr->sin_addr) == 0)
		error("inet_aton() failed");

    
	// my Address
	memset(&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myAddr.sin_port = htons((unsigned short)atoi(clntPort));
	//if (inet_aton(servIP, &myAddr.sin_addr) == 0)
	//	error("inet_aton() failed");


	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		error("socket() failed");
		
	// non blocking Socket will test server/peer's status
	if ((mySock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		error("socket() failed");
	
	//setsockopt(mySock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
	
	if (bind(mySock, (struct sockaddr *)&myAddr, sizeof(myAddr)) < 0)
		error("ERROR on bind() 1");

	//Blocking socket that will wait for changes in Client Table
/*
	if ((servSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		error("socket() failed");

	setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
	if (bind(servSock, (struct sockaddr *)&myAddr, sizeof(myAddr)) < 0)
		error("ERROR on bind() 2");
*/
	// creating socket that times out..To be used to test clients
	// and server status
/*
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500 * 1000;
	setsockopt(mySock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
*/

	//set peersock vals identical

	initTABLE();
	struct entry *T = TABLE;
	


	snprintf(buf, BUFSIZE, "[INIT]%s", uname);
	
	fprintf(stderr, "sending %s to server\n", buf);


	n = sendto(mySock, buf, strlen(buf) + 1, 0, 
						(struct sockaddr *)servAddr, servlen);
	
	
	
	pthread_create(&tid, NULL, usrRequest, servAddr);
	fprintf(stderr, "INIT sent to server\n");
	
	if (n < 0)
		error("ERROR sending packet to server");

	if (getACK(buf, mySock, &clntAddr, &mylen) == 0)
		die(">>> [Server not responding]\n>>> [Exiting]");

	//pthread_create(&tid, NULL, usrRequest,args);
	
	for(; ;) {
		pthread_mutex_lock(&mutex);
		n = recvfrom(mySock, buf, BUFSIZE, 0,
						(struct sockaddr *)&clntAddr, &servlen);

		pthread_mutex_unlock(&mutex);
		
		if (*buf == 0)
			continue;
			
		if (strstr(buf, "[UPDATE]") == buf) {
			currSz = atoi(strstr(buf, "]") + 1);
			fprintf(stderr, "new table of size %d\n", currSz);
			if (currSz > maxSz - 1)
				resizeTABLE();
			for (i = 0; i < currSz; i++) 
				n = recvfrom(mySock, &T[i], sizeof(struct entry), 0,
						(struct sockaddr *)&clntAddr, &servlen);
			for (i = 0; i < currSz; i++)
				fprintf(stderr, "%d: %s %s %u %d\n", 
						i,
						T[i].uname, 
						inet_ntop(AF_INET, &(T[i].clntaddr.sin_addr.s_addr), ipstr, 20), 
						ntohs(T[i].clntaddr.sin_port),
						T[i].status);
			
			fprintf(stderr,">>>[Client table updated]\n>>>");
		}
		
		memset(buf, 0, BUFSIZE);
	}
	
}


int getACK(char *buf, int servSock, struct sockaddr_in *servAddr, socklen_t *len)
{
	fprintf(stderr, "In ACK\n");
	int i, n; 

	i = 0;	
	while (i++ < 5) {
		ualarm(500*1000, 0);
		n = recvfrom(servSock, buf, BUFSIZE, 0,
							(struct sockaddr *)servAddr, len);
		if (n > 0 && strcmp(buf, "ACK") == 0)
			return 1;
	}
	return 0;
}

void broadcastTABLE(void)
{
	int i,clntSock, j;
	char msg[30];
	struct entry *T;
	socklen_t clntlen;	
	
	T = TABLE;
	i = 0;	
	clntlen = sizeof(struct sockaddr_in);
	sprintf(msg, "[UPDATE]%d", currSz);
	
	if ((clntSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		error("socket() failed");

	for (i = 0; i < currSz; i++) {
		if (T[i].status == 1) {
			sendto(clntSock, msg, strlen(msg)+1, 0,(struct sockaddr *)&(T[i].clntaddr)						, clntlen);
		
			for (j = 0; j < currSz; j++) {
				sendto(clntSock, &T[j], sizeof(struct entry), 0,
								(struct sockaddr *)&(T[i].clntaddr), clntlen);
			}
		}
	}		
}

int findEntry(char *name)
{
	int i = 0;
	while (i < currSz) {
		if (strcmp(TABLE[i].uname, name) == 0)
			return i;
		i++;
	}
	return -1;
}

void initTABLE(void)
{
	TABLE = malloc(maxSz*sizeof(struct entry));
}

void resizeTABLE(void)
{
	maxSz *= 2;
	struct entry *tmp = malloc(maxSz*sizeof(struct entry));
	memcpy(tmp, TABLE, (maxSz/2)*sizeof(struct entry));
	free(TABLE);
	TABLE = tmp;
}

void addEntry(char *uname, struct sockaddr_in *clntaddr)
{	
	struct entry *T = TABLE;

	strcpy(T[currSz].uname, uname);
	memcpy(&(T[currSz].clntaddr), clntaddr, sizeof(struct sockaddr_in));
	T[currSz].status = 1;
	
	if (currSz == maxSz - 1)
		resizeTABLE();	
	currSz++;
}




