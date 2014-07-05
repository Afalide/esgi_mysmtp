
#include "mysmtp.h"

int main(int argc, char** argv)
{
	int					result;
	int					maxConnections = 10;
	int					serverSocket;
	int					serverPort;
	const char*			serverIP = "127.0.0.1";
	struct sockaddr_in 	serverSockaddr;
	socklen_t 			serverSockaddrSize;

	//Scan the we will listen to
	if(argc < 2)
	{
		printf("Wrong number of arguments\n");
		return 1;
	}


	//Get the port
	serverPort = (int) strtol(argv[1],NULL,10);
    if(0 == serverPort)
    {
        printf("Port value (%s) is wrong\n",argv[1]);
        return 1;
    }


	//Create a socket
	mslog("Creating socket...");
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == serverSocket){
		mslog("Fail\n");
		return 1;
	}
	mslog("Ok\n");


	//Fill the structure with the port and a numeric version of the IP
	mslog("Creating server IP infos...");
	serverSockaddr.sin_family = AF_INET;
	serverSockaddr.sin_port = htons(serverPort);
	result = inet_pton(AF_INET, serverIP, &serverSockaddr.sin_addr);
	if(result <= 0){
		mslog("Fail of inet_pton\n");
		return 1;
	}
	mslog("Ok\n");


	//Now we bind the server address to the socket
	mslog("Binding server's IP to socket...");
	serverSockaddrSize = sizeof(serverSockaddr);
	result = bind(serverSocket,(struct sockaddr*) &serverSockaddr, serverSockaddrSize);
	if(result != 0){
		mslog("Fail\n");
		return 1;
	}
	mslog("Ok\n");


	//We transform the socket in a passive socket
	mslog("Putting socket on listen mode...");
	result = listen(serverSocket, maxConnections);
	if(result != 0){
		mslog("Fail\n");
		return 1;
	}
	mslog("Ok\n");


	//Declare that we ignore child's signals
	//This is needed beaceaus we will fork the server
	//for each client, and we do not want zombies
	//(we have not enough shotgun shells)
	signal(SIGCHLD,SIG_IGN);


	//Now we can accept connections.
	//When a client connects, we fork this server.
	//The parent will kepp accepting clients, and the
	//child will handle the mail transfer with the
	//client
	printf("Server setup done. Starting connections wait loop\n");
	struct sockaddr_in 	clientSockaddr;
	socklen_t 			clientSockaddrSize = sizeof(clientSockaddr);
	int 				clientSocket = 0;
	int 				forkPid;
	mscn* 				cnClient = NULL;


	while((clientSocket = accept(serverSocket
	                             ,(struct sockaddr*)&clientSockaddr
								 , &clientSockaddrSize)))
	{
		forkPid = fork();
		if(forkPid != 0)
		{
			//We are the parent server. We make a new loop to
			//accept new clients
			mslog("Server(P) created child %d\n",forkPid);
			continue;
		}else{
			//We are the child server. We are now connected to a client.
			mslog("Server(C) is now connected with a client\n");
			break;
		}
	}


	//This part is only accessed by forked children (parent is stuck
	//in the loop)
	printf("Server socket is %d\n",serverSocket);
	printf("Client socket is %d\n",clientSocket);

	cnClient = msCatch(clientSocket, &clientSockaddr);
	mslog("Server(C) catched connection: %p\n",cnClient);

	msSendString("Hey you!",cnClient);
	//int sent = write(cnClient->socket, "hey you", strlen("hey you"));

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


int main(){
	printf("Server start\n");
	int result = 0;

	printf("Creating socket...");
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == server_socket){
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
	result = inet_pton(AF_INET, server_ip, &server_sockaddr.sin_addr); //Convertit l'IP en version numerique et la stocke dans sin_addr
	if(result <= 0){
		printf("Fail of inet_pton\n");
		return 1;
	}
	printf("Ok\n");

	printf("Binding server's IP to socket...");
	socklen_t server_sockaddr_size = sizeof(server_sockaddr);
	server_sockaddr_size = sizeof(server_sockaddr);
	result = bind(server_socket,(struct sockaddr*) &server_sockaddr, server_sockaddr_size);
	if(result != 0){
		printf("Fail\n");
		return 1;
	}
	printf("Ok\n");

	printf("Putting socket on listen mode...");
	int max_connections = 10;
	result = listen(server_socket, max_connections);
	if(result != 0){
		printf("Fail\n");
		return 1;
	}
	printf("Ok\n");

	printf("All done. Starting connections wait loop\n");
	struct sockaddr_in 	client_sockaddr;
	socklen_t 			client_sockaddr_size;
	int 				client_socket = 0;

	while((client_socket = accept(server_socket
	                             ,(struct sockaddr*)&client_sockaddr
								 , &client_sockaddr_size)))
	{
		if(client_socket == -1){
			printf("Error on accept() call\n");
			break;
		}

		char 	client_ip[20];
		int		client_port = client_sockaddr.sin_port;
		inet_ntop(AF_INET, &client_sockaddr.sin_addr, client_ip, sizeof(client_ip));
		printf("Got a client connection (%s:%d).\n",client_ip,client_port);

		printf("Reading client's data...");
		char buffer[100];
		int nread = read(client_socket, buffer, 99);
		if(nread == -1){
			printf("Fail\n");
			return 1;
		}
		if(nread == 0){
			printf("Client ended connection.\n");
			continue;
		}
		buffer[nread] = '\0';
		printf("Recieved data [%s]\n",buffer);

		printf("Echoing data...");
		int nsent = write(client_socket, buffer, strlen(buffer));
		if(nsent == -1){
			printf("Fail\n");
			return 1;
		}
		printf("Ok\n");
	}

	if(client_socket == -1){
		printf("Error on accept()\n");
		return 1;
	}

	printf("Server end\n");
	return 0;
}
*/
