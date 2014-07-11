
#include "mysmtp.h"

int main(int argc, char** argv)
{
	/////
//	const char* buf = "\r\n.\r\n";
//	printf("%d\n[%s]\n",strlen(buf),buf);
//	return 0;
	/////

	int					result;
	int					maxConnections = 10;
	int					serverSocket;
	int					serverPort;
	char*				serverIP;
	struct sockaddr_in 	serverSockaddr;
	socklen_t 			serverSockaddrSize;

	//Scan the we will listen to
	if(argc < 3)
	{
		printf("Wrong number of arguments\n");
		return 1;
	}


	//Get the port
	serverPort = (int) strtol(argv[2],NULL,10);
    if(0 == serverPort)
    {
        printf("Port value (%s) is wrong\n",argv[2]);
        return 1;
    }

    //Get the IP
    serverIP = argv[1];
    printf("Starting MySmtp server on %s : %d\n",serverIP,serverPort);

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
	//This is needed because we will fork the server
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
	mscn* 				clientCn = NULL;


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
			//We also close the serverSocket, it is not the child's job
			//to listen on connections...
			close(serverSocket);
			break;
		}
	}


	//This part is only accessed by forked children (parent is stuck
	//in the loop)
	clientCn = msCatch(clientSocket, &clientSockaddr);
	mslog("Server socket is %d\n",serverSocket);
	mslog("Client socket is %d\n",clientSocket);
	mslog("Server(C) caught connection: %p\n",clientCn);

	//Let's send a welcome message...
	msSendString("220 Welcome to the MySMTP server",clientCn);

	//We use select(), so we can wait on multiple fd (if needed)
	//rather than blocking on the client socket
	fd_set set;
	FD_ZERO(&set);

	while(1)
	{
		mslog("Calling select...");
		FD_ZERO(&set);
		FD_SET(clientSocket, &set);
		result = select(clientSocket+1, &set, NULL, NULL, NULL);

		//select() returned: check where the event is
		if(result == -1)
		{
			mslog("Error, select() returned -1\n");
			return 1;
		}
		mslog("OK, event(s) found\n");


		if(FD_ISSET(clientSocket,&set))
		{
			//Event found on the client. Check what must be done
			char* clientStr = msReadString(clientCn);
			if(NULL == clientStr)
			{
				//Client ended connection
				mslog("Client ended connection, nothing to read, child server will exit\n");
				break;
			}

 			//SMTP command: EHLO
			//The EHLO command identifies the client with a name
			//It is mandatory to issue an EHLo before any other command
			if(msStartsWith(clientStr,"EHLO"))
			{
				char* ehloName = msGetParameter(clientStr,"EHLO");
				if(NULL == ehloName)
				{
					ehloName = (char*)malloc(20);
					strcpy(ehloName, "<no-name-client>");
					ehloName[strlen(ehloName)] = '\0';
				}
				clientCn->cmd_ehlo = ehloName;
				msSendString("250 Hello, random citizen. MySmtp ready",clientCn);
				mslog("SMTP EHLO recieved from %s at %s:%d\n",clientCn->cmd_ehlo
															 ,clientCn->host
															 ,clientCn->port);
				continue;
			}

			//SMTP command: MAIL FROM
			if(msStartsWith(clientStr,"MAIL FROM:"))
			{
				if(!clientCn->cmd_ehlo)
				{
					msSendString("503 Must issue EHLO first",clientCn);
					continue;
				}

				char* mailFrom = msGetParameter(clientStr,"MAIL FROM:");
				if(NULL == mailFrom)
				{
					msSendString("501 No reverse path specified",clientCn);
					continue;
				}

				if(!msStartsWith(mailFrom,"<") || !msEndsWith(mailFrom,">\r\n"))
				{
					msSendString("501 Incorrect format (must enclose with < >)",clientCn);
					continue;
				}

				clientCn->cmd_mailfrom = mailFrom;
				msSendString("250 Ok",clientCn);
				mslog("Client %s:%d filled mail from %s\n",clientCn->host
														  ,clientCn->port
														  ,clientCn->cmd_mailfrom);
				continue;
			}

			//SMTP command: RCPT TO
			if(msStartsWith(clientStr,"RCPT TO:"))
			{
				if(!clientCn->cmd_mailfrom)
				{
					msSendString("503 Must issue MAIL FROM before RCPT TO",clientCn);
					continue;
				}

				char* rcptTo = msGetParameter(clientStr,"RCPT TO:");
				if(NULL == rcptTo)
				{
					msSendString("501 No destination specified",clientCn);
					continue;
				}

				if(!msStartsWith(rcptTo,"<") || !msEndsWith(rcptTo,">\r\n"))
				{
					msSendString("501 Incorrect format (must enclose with < >)",clientCn);
					continue;
				}

				clientCn->cmd_rcptto = rcptTo;
				msSendString("250 Ok",clientCn);
				continue;
			}

			//SMTP command: DATA
			if(msStartsWith(clientStr, "DATA"))
			{
				if(!clientCn->cmd_rcptto)
				{
					msSendString("503 Must issue RCPT TO before DATA",clientCn);
					continue;
				}

				clientCn->cmd_data_isComposing = 1;
				clientCn->cmd_data = (char*) malloc (1);
				clientCn->cmd_data[0] = '\0';
				msSendString("354 The NSA will hear about that",clientCn);
				continue;
			}

			//Before throwing an error, let's verify if the client is composing a
			//mail. So everything he is sending must be interpreted as a mail DATA.
			if(clientCn->cmd_data_isComposing)
			{
				int lenConcatData = strlen(clientCn->cmd_data) + strlen(clientStr);
				char* concatData = (char*) malloc (lenConcatData+1);
				strcpy(concatData, clientCn->cmd_data);
				strcat(concatData, clientStr);
				concatData[lenConcatData] = '\0';
				free(clientCn->cmd_data);
				clientCn->cmd_data = concatData;

				//Check if the last end dot is sent (means end of DATA stream)
				if(msEndsWith(concatData,"\r\n.\r\n"))
				{
					mslog("Will send Mail containing [%s][%s][%s]\n",clientCn->cmd_mailfrom
					                                                ,clientCn->cmd_rcptto
					                                                ,clientCn->cmd_data);
					msSendString("250 Ok, mail queued for sending",clientCn);

					//Send mail here
					const char*		cmdMailFrom = "MAIL FROM:";
					const char*		cmdRcptTo = "RCPT TO:";
					char* 			cmdMailFromParam = (char*)malloc(strlen(cmdMailFrom)+strlen(clientCn->cmd_mailfrom)+1);
					char* 			cmdRcptToParam =   (char*)malloc(strlen(cmdRcptTo)  +strlen(clientCn->cmd_rcptto)+1);
					mscn* gmailCn = msConnect("smtp.gmail.com",587,1);

					//Send EHLO
					msSendString("EHLO mysmtp.esgi",gmailCn);
					free(msReadString(gmailCn));

					//Authenticate
					msSendString("AUTH LOGIN",gmailCn);
					free(msReadString(gmailCn));
					msSendString(msEncodeBase64("mysmtp.esgi@gmail.com"),gmailCn);
					free(msReadString(gmailCn));
					msSendString(msEncodeBase64("Esg1 Smtp"),gmailCn);
					free(msReadString(gmailCn));

					//Send MAIL FROM
					strcpy(cmdMailFromParam,cmdMailFrom);
					strcat(cmdMailFromParam,clientCn->cmd_mailfrom);
					cmdMailFromParam[strlen(cmdMailFromParam)] = '\0';
					msSendStringNoCrlf(cmdMailFromParam,gmailCn);
					//msSendString("MAIL FROM:<plop@toast.com>",gmailCn);
					free(msReadString(gmailCn));
					free(cmdMailFromParam);

					//Send RCPT TO
					strcpy(cmdRcptToParam,cmdRcptTo);
					strcat(cmdRcptToParam,clientCn->cmd_rcptto);
					cmdRcptToParam[strlen(cmdRcptToParam)] = '\0';
					msSendStringNoCrlf(cmdRcptToParam,gmailCn);
					//msSendString("RCPT TO:<romain.notari@gmail.com>",gmailCn);
					free(msReadString(gmailCn));
					free(cmdRcptToParam);

					//Send DATA
					msSendString("DATA",gmailCn);
					free(msReadString(gmailCn));
					msSendString(clientCn->cmd_data,gmailCn);
					//msSendString("Message contents\n\r\n.\r\n",gmailCn);
					free(msReadString(gmailCn));

					break;
				}
				continue;
			}

			//Nothing caught, so the client command must be an error
			//Let's throw an "unimplemented command" error
			mslog("Client is composing = %d\n",clientCn->cmd_data_isComposing);
			msSendString("502 Command not implemented",clientCn);
			free(clientStr);
		}

		else
		{
			mslog("Event found, but it is not on the client socket\n");
			continue;
		}
	}

	close(clientSocket);
	mslog("Server(C) terminated!\n");

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
