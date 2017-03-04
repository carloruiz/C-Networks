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

//#include <pthreads.h>


#define BUFSIZE 1024

void udpServer(char *);
void udpClient(char *, char *, char *, char *);

void error(char *msg) {
    perror(msg);
    exit(1);
}

void die(char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

struct servthread_args {
    char *msg;
    struct sockaddr_in clntaddr;
};

void serverRequest(void *ptr)
{
	struct servthread_args *args = (struct servthread_args *)ptr;
	
}

int main(int argc, char **argv) {

	if (argc < 2)
		error("Insufficient args");
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
	fprintf(stderr, "In server\n\n");
    int servSock, port;
    struct sockaddr_in servaddr, clntaddr; //tmp
    struct hostent *hostp;
    char buf[BUFSIZE];
    char *hostaddrp;
    int n;
    socklen_t clntlen;

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

	fprintf(stderr, "server up..LESSGOOO..GET THIS CHAT GOIIN\n");

    clntlen = sizeof(struct sockaddr_in);

    for ( ; ;) {

    /*  tmp = malloc(sizeof(struct sockaddr_in));
        memcpy(tmp, clntaddr, sizeof(clntaddr));
        temp->msg = malloc(BUFSIZE);
        recvfrom(servSock, temp->msg, BUFSIZE etc...
    */
        //buf = malloc(BUFSIZE);
		
		fprintf(stderr, "wating on client\n");

        n = recvfrom(servSock, buf, BUFSIZE, 0,
                        (struct sockaddr *)&clntaddr, &clntlen);
		
        if (n < 0) {
            perror("ERROR on recvfrom()");
            continue;
        }
		
		fprintf(stderr, "recieved %s from client\n", buf); 
			
        if (sendto(servSock, "ACK", 4, 0, (struct sockaddr *)&clntaddr, clntlen) < 4) {
			perror("sending ACK failed\n");
            continue;
        }

		fprintf(stderr, "ACK sent to client\n");
        //buf[BUFSIZE - 1] = '\0';
        //pthread_create(&tid, NULL, serverRequest, temp);
    }
}

void udpClient(char *uname, char *servIP, char *servPort, char *clntPort)
{
	fprintf(stderr, "In client\n\n");
	fprintf(stderr, "%s\n%s\n%s\n%s\n", uname, servIP, servPort, clntPort);

	
	int servSock, mySock, port, n,i;
	struct sockaddr_in servAddr, myAddr;
	char buf[BUFSIZE];
	socklen_t servlen, mylen;

	servlen = mylen =  sizeof(struct sockaddr_in);
	
	// Creating my socket and binding it to given port
	if ((mySock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		error("socket() failed");

	memset(&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_port = htons(atoi(clntPort));
	if (inet_aton(servIP, &myAddr.sin_addr) == 0)
		error("inet_aton() failed");

	if (bind(mySock, (struct sockaddr *)&myAddr, sizeof(myAddr)) < 0)
		error("ERROR on bind()");

	// Creating server socket with given args
	if ((servSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		error("socket() failed");

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(atoi(servPort));
	if (inet_aton(servIP, &servAddr.sin_addr) == 0)
		error("inet_aton() failed");

	//set servSock time out to 500 millies
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500 * 1000;
	setsockopt(servSock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

	snprintf(buf, BUFSIZE, "%s|%s", uname, clntPort);
	
	fprintf(stderr, "sending %s to server\n", buf);


	n = sendto(servSock, buf, strlen(buf) + 1, 0, (struct sockaddr *)&servAddr, servlen);
	if (n < 0)
		error("ERROR sending packet to server");

	i = 0;
	while (i++ < 5) {
		fprintf(stderr, "Waiting for ACK: %d\n", i);
		n = recvfrom(servSock, buf, BUFSIZE, 0,
							(struct sockaddr *)&servAddr, &mylen);
		if (n > 0)
			break;
	}

	if (i == 6)
		die(">>> [Server not responding]\n>>> [Exiting]");

	if (strcmp(buf, "ACK") != 0)
		error("No acknowldgement recieved from server");
	
	fprintf(stderr, "connection successful\n");

	exit(1);
	
}

