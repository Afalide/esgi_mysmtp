#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	
	printf("Sending data to server...");
	const char* data = "Hello";
	int sent = write(client_socket, data, strlen(data));
	if(sent == -1){
		printf("Fail\n");
		return 1;
	}
	printf("Ok\n");

	printf("Awaiting response...");
	char buffer[100];
	int nread = read(client_socket, buffer, 99);
	if(nread == -1){
		printf("Fail\n");
		return 1;
	}
	if(nread == 0)
		printf("Server ended connection. ");
	buffer[nread] = '\0';
	printf("Recieved data [%s]\n",buffer);
	
	
	/*printf("Reading data on socket...");
	char buffer[100];
	int amountread = read(client_socket, buffer, 100);
	buffer[amountread] = '\0';
	if(amountread != 0){
		printf("%s\n",buffer);
	}else{
		printf("Fail\n");
	}*/
	
	printf("Client end\n");
	return 0;
}
