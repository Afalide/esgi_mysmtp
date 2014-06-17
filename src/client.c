#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define max(X,Y) X>Y ? X : Y

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

	/*
	printf("Creating a FDset with stdin and socket...");
	fd_set set;
	FD_ZERO(&set);
	FD_SET(client_socket, &set);
	FD_SET(0, &set); //(0 == stdin's file descriptor)
	printf("Ok\n");

	printf("Please input data to send: ");
	select( max(client_socket,0),
		&set,
		NULL,
		NULL,
		NULL);
	*/
	
	printf("Please input data to send: ");
	char buf[256];
	fgets(buf, sizeof(buf), stdin);
	buf[strlen(buf)-1] = '\0'; //Remplace le \n final par un \0

	printf("Sending data to server...");
	int sent = write(client_socket, buf, strlen(buf));
	if(sent == -1){
		printf("Fail\n");
		return 1;
	}
	printf("Ok\n");

	printf("Awaiting response...");
	int nread = read(client_socket, buf, sizeof(buf));
	if(nread == -1){
		printf("Fail\n");
		return 1;
	}
	if(nread == 0){
		printf("Server ended connection.\n");
		return 1;
	}
	buf[nread] = '\0';
	printf("Recieved data [%s]\n",buf);
	
	printf("Client end\n");
	return 0;
}
