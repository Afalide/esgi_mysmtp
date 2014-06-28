#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

int msCreateSocket(int* fdSocket);
int msConnect(const char* url, int port, int fdSocket, int encrypted);
int msSendString(const char* str, int fdSocket);
int msReadString(char** str, int fdSocket);

int msCreateSocket(int* fdSocket)
{
    printf("Allocating socket...");
    if(NULL == fdSocket)
    {
        printf("Fail (nullptr)\n");
        return 0;
    }
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == sock)
    {
        printf("Fail\n");
        return 0;
    }

    *fdSocket = sock;
    printf("Ok\n");
    return 1;
}

int msConnect(const char* url, int port, int fdSocket, int encrypted)
{
    printf("Resolving %s...",url);
    struct hostent* host;
    host = gethostbyname(url);
    if(NULL == host)
    {
        printf("Fail (gethostbyname())\n");
        return 0;
    }
    struct in_addr**    solvedAddresses = (struct in_addr**) host->h_addr_list;
    struct in_addr*     solvedFirstAddress = solvedAddresses[0];
    char*               ipPres = inet_ntoa(*solvedFirstAddress);
    printf("Ok (solved to %s)\n",ipPres);

    printf("Connecting to %s:%d...",ipPres,port);
    fflush(stdout);
    {
        struct sockaddr_in      server_sockaddr;
        socklen_t                server_sockaddr_size = sizeof(server_sockaddr);

        server_sockaddr.sin_family = AF_INET;
        server_sockaddr.sin_port   = htons((unsigned short) port);
        server_sockaddr.sin_addr = *solvedFirstAddress;

        if(connect(fdSocket, (struct sockaddr*)&server_sockaddr, server_sockaddr_size) != 0)
        {
            printf("Fail (connect())\n");
            return 0;
        }
    }
    printf("Ok\n");
    return 1;
}

int msSendString(const char* str, int fdSocket)
{
    printf("Sending data [%s]...",str);
    fflush(stdout);

    int sent = write(fdSocket, str, strlen(str));
    if(sent == -1){
        printf("Fail (write())\n");
        return 0;
    }
    printf("Ok\n");
    return 1;
}

int msReadString(char** str, int fdSocket)
{
    //read socket data
    printf("Reading server message...");
    fflush(stdout);

    char* buffer = malloc(1024);
    int nread = read(fdSocket, buffer, 1024);

    //read failed?
    if(nread == -1){
        printf("Fail (read())\n");
        return 0;
    }

    //server killed?
    else if(nread == 0){
        printf("Fail (server ended connection)\n");
        return 0;
    }

    //server wrote something?
    else{
        buffer[nread] = '\0';
        printf("Ok (recieved [%s])\n",buffer);
    }

    *str = buffer;
    return 1;
}

int main()
{
    const char* url = "smtp.gmail.com";
    int port = 587;
    int serverSocket;

    if(!msCreateSocket(&serverSocket))
    {
        printf("Failed to create socket\n");
        return 1;
    }

    if(!msConnect(url,port,serverSocket,1))
    {
        printf("Failed to connect\n");
        return 1;
    }

    if(!msSendString("EHLO",serverSocket))
    {
        printf("Send failed\n");
        return 1;
    }

    char* str = NULL;
    if(!msReadString(&str, serverSocket))
    {
        printf("Nothing read\n");
        return 1;
    }
    free(str);

    printf("Response from server: [%s]\n",str);
    return 0;
}






/*
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define max(x,y) ((x)>(y)?(x):(y))

int main(int argc, char** argv)
{
    const char* connectionString = "smtp.gmail.com:487";

    printf("Connecting to %s...\n",connectionString);

    return 0;
}
*/

/*
int main(){
	printf("Client start\n");
	int result = 0;

	printf("Creating socket...");
	int client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == client_socket){
		printf("Fail\n");
		return 1;
	}
	printf("Ok\n");

	printf("Creating server IP infos...");
	struct sockaddr_in 	server_sockaddr;
	const char* 		server_ip = "127.0.0.1";
	short 				server_port = htons(3000);

	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_port = server_port;
	in_port_t toast;
	result = inet_pton(AF_INET, server_ip, &server_sockaddr.sin_addr);
	if(result <= 0){
		printf("Fail of inet_pton\n");
		return 1;
	}
	printf("Ok\n");

	printf("Connecting to server %s:3000...", server_ip);
	socklen_t server_sockaddr_size = sizeof(server_sockaddr);
	server_sockaddr_size = sizeof(server_sockaddr);

	result = connect(client_socket, (struct sockaddr*)&server_sockaddr, server_sockaddr_size);
	if(result != 0){
		printf("Fail\n");
		return 1;
	}
	printf("Ok\n");

	//infinite loop, only exited by a CTRL+C (while true)
	while(1){
		//build file descriptor set
		fd_set set;
		FD_ZERO(&set);
		FD_SET(STDIN_FILENO, &set);
		FD_SET(client_socket, &set);

		//select call
		printf("Waiting for events...\n");
		int maxfdp1 = max(client_socket,STDIN_FILENO) +1;
		result = select(maxfdp1, &set, NULL, NULL, NULL);

		//select returned: check where the event is
		if(result == -1){
			printf("Error, select() returned -1\n");
			return 1;
		}

		//is it a socket event?
		if(FD_ISSET(client_socket, &set)){
			printf("Event found on socket\n");

			//read socket data
			printf("Reading server message...");
			char buf[256];
			int nread = read(client_socket, buf, sizeof(buf));
			fflush(stdin);

			//read failed?
			if(nread == -1){
				printf("Fail (-1)\n");
				return 1;
			}

			//server killed?
			else if(nread == 0){
				printf("Server ended connection.\n");
				return 1;
			}

			//server wrote something?
			else{
				buf[nread] = '\0';
				printf("Recieved data: [%s]\n",buf);
			}
		}

		//is it an stdin event?
		if(FD_ISSET(STDIN_FILENO, &set)){
			printf("Event found on stdin\n");

			//read stdin
			char buf[256];
			fgets(buf, sizeof(buf), stdin);
			buf[strlen(buf)-1] = '\0'; //replace the final '\n' with '\0'

			//send it to the server
			printf("Sending [%s] to server...",buf);
			int sent = write(client_socket, buf, strlen(buf));
			if(sent == -1){
				printf("Fail (-1)\n");
				return 1;
			}
			printf("Ok\n");
		}
	}

	printf("Client end\n");
	return 0;
}
*/


