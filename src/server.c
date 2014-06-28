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
