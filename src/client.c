#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
//#include <openssl/err.h>
//#include <openssl/pem.h>

//////////////////////////////////////////
//The MySmtp Connection structure
//////////////////////////////////////////

typedef struct mscn_s
{
    //Common data
    int             socket;
    int             usesSsl;
    char*           host;
    int             port;

    //SSL data
    const SSL_METHOD*   sslMethod;
    SSL_CTX*            sslContext;
    SSL*                sslConnection;
} mscn;

//////////////////////////////////////////
//MySmtp functions
//////////////////////////////////////////

mscn*           msConnect(const char* host, int port, int useSsl);
void            msSendString(const char* str, mscn* cn);
void            msSendStringNoCrlf(const char* str, mscn* cn);
char*           msEncodeBase64(const char* str);
char*           msReadString(mscn* cn);
void            msPrintSSLError(SSL* sslConnection, int sslReturnCode);
void            msStrReplace(char* source, char target, char replacement);

//////////////////////////////////////////
//msConnect
//Creates an mscn structure, which is used by the ms* functions.
//////////////////////////////////////////

mscn*
msConnect(const char* host, int port, int useSsl)
{
    mscn* ret = (mscn*) malloc(sizeof(mscn));

    //Solve the url, we need an IP
    printf("Resolving %s...",host);
    struct hostent* hostSolve;
    hostSolve = gethostbyname(host);
    if(NULL == hostSolve)
    {
        printf("Fail (gethostbyname())\n");
        return 0;
    }

    //Many IP's may be found. We only take the first.
    struct in_addr**    solvedAddresses = (struct in_addr**) hostSolve->h_addr_list;
    struct in_addr*     solvedFirstAddress = solvedAddresses[0];
    struct sockaddr_in  server_sockaddr;
    socklen_t            server_sockaddr_size = sizeof(server_sockaddr);
    char*               ipPres = inet_ntoa(*solvedFirstAddress);
    printf("Ok (solved to %s)\n", ipPres);

    //Fill the server address structure with useful infos
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port   = htons((unsigned short) port);
    server_sockaddr.sin_addr = *solvedFirstAddress;

    //Create the socket
    printf("Allocating socket...");
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == sock)
    {
        printf("Fail\n");
        return 0;
    }
    printf("Ok\n");

    //Connect to the server
    printf("Connecting to %s:%d...",ipPres,port);
    fflush(stdout);
    if(connect(sock, (struct sockaddr*)&server_sockaddr, server_sockaddr_size) != 0)
    {
        printf("Fail\n");
        return 0;
    }
    printf("Ok\n");

    ret->socket = sock;
    ret->usesSsl = useSsl;
    ret->host = (char*) malloc(strlen(host));
    ret->port = port;
    strcpy(ret->host, host);
    ret->sslConnection = NULL;
    ret->sslContext = NULL;
    ret->sslMethod = NULL;

    if(1 == useSsl)
    {
        printf("Loading SSL...");
        fflush(stdout);
        const SSL_METHOD*   sslMethod;
        SSL_CTX*            sslContext;
        SSL*                sslConnection;

        //Load the SSL library
        OpenSSL_add_all_algorithms();
//        ERR_load_BIO_strings();
//        ERR_load_crypto_strings();
//        SSL_load_error_strings();

        if(SSL_library_init() < 0)
        {
            printf("Failed\n");
            return 0;
        }
        printf("Ok\n");

        //We will use TLS
        printf("Declaring TLSv1...");
        //sslMethod = SSLv23_client_method();
        sslMethod = TLSv1_client_method();
        if(NULL == sslMethod)
        {
            printf("Fail\n");
            return 0;
        }
        printf("Ok\n");

        //We create an SSL context (it will be associated to the socket)
        printf("Creating SSL context...");
        sslContext = SSL_CTX_new(sslMethod);
        if(NULL == sslContext)
        {
            printf("Fail\n");
            return 0;
        }
        printf("Ok\n");

        //This disables SSL2 for the context to force SSL3 negotiation
        //SSL_CTX_set_options(sslContext, SSL_OP_NO_SSLv2);

        //We create an SSL structure
        printf("Creating connection structure...");
        sslConnection = SSL_new(sslContext);
        if(NULL == sslConnection)
        {
            printf("Fail\n");
            return 0;
        }
        printf("Ok\n");

        //This associates the SSL context with the socket
        SSL_set_fd(sslConnection, sock);

        //Fill the structure we will return
        ret->sslConnection = sslConnection;
        ret->sslContext = sslContext;
        ret->sslMethod = sslMethod;
        ret->usesSsl = 0; //This is to enforce a no-ssl write/read for
                          //the ehlo / starttls commands
                          //we will set it back to 1 later

        //Before using TLS we must issue a STARTTLS command to the smtp server
        //First, flush the welcome message
        free(msReadString(ret));
        //Send EHLO
        msSendString("ehlo",ret);
        //Flush again the response
        free(msReadString(ret));
        //Send STARTTLS
        msSendString("starttls",ret);
        //Flush again the response
        free(msReadString(ret));

        //Now we can connect with SSL (this makes a "SSL handshake")
        printf("Establishing SSL session...");
        fflush(stdout);
        int sslConnectResult = SSL_connect(sslConnection);
        if(sslConnectResult != 1)
        {
            printf("Fail\n");
            msPrintSSLError(sslConnection, sslConnectResult);
            return 0;
        }
        printf("Ok\n");

        //From now, everything that is sent and read trough the socket
        //is encrypted (msReadString() and msSendString() will act differently)
        ret->usesSsl = 1; //This modifies the behavior of the read/write functions
    }

    return ret;
}

//////////////////////////////////////////
//msSendString
//Sends a single string to an host
//////////////////////////////////////////

void
msSendString(const char* str, mscn* cn)
{
    int sent = -1;
    int strLen = strlen(str);
    char* strWithCrlf = (char*) malloc(strLen + 3);

    //We append a CRLF to the string, this is to ensure
    //that the smtp server will recieve correct separated commands
    strcpy(strWithCrlf, str);
    strcat(strWithCrlf, "\r\n");

    printf("Sending data [%s]...",str);
    fflush(stdout);

    //If we use SSL, then write() must be done by the SSL library
    if(cn->usesSsl == 1)
    {
        sent = SSL_write(cn->sslConnection, strWithCrlf, strlen(strWithCrlf));
    }
        else
    {
        sent = write(cn->socket, strWithCrlf, strlen(strWithCrlf));
    }

    if(sent == -1)
    {
        printf("Fail (write())\n");
        return;
    }
        else if (sent == 0)
    {
        printf("Fail (0 bytes sent)\n");
        return;
    }
    printf("Ok\n");
}

void
msSendStringNoCrlf(const char* str, mscn* cn)
{
    int sent = -1;

    printf("Sending data [%s] without CRLF...",str);
    fflush(stdout);

    //If we use SSL, then write() must be done by the SSL library
    if(cn->usesSsl == 1)
    {
        sent = SSL_write(cn->sslConnection, str, strlen(str));
    }
        else
    {
        sent = write(cn->socket, str, strlen(str));
    }

    if(sent == -1)
    {
        printf("Fail (write())\n");
        return;
    }
        else if (sent == 0)
    {
        printf("Fail (0 bytes sent)\n");
        return;
    }
    printf("Ok\n");
}

//////////////////////////////////////////
//msEncodeBase64
//Encodes the specified string and returns it's Base64 version
//////////////////////////////////////////

char*
msEncodeBase64(const char* str)
{
    BIO* base64Bio;
    BIO* memoryBio;

    //Create a Base64 BIO (BIOs are filters that transforms streams
    //they can also write and read to file descriptors)
    base64Bio = BIO_new(BIO_f_base64());

    //Create a memory BIO (it will write to a buffer (a char array),
    //rather than stdout or a file)
    memoryBio = BIO_new(BIO_s_mem());

    //Add the base64 filter to the memory BIO.
    memoryBio = BIO_push(base64Bio, memoryBio);

    //Write the string to the mem BIO. It will be encoded to Base64
    BIO_write(base64Bio, str, strlen(str));
    if(BIO_flush(base64Bio) != 1)
        return NULL;

    //Get the result string. It is located in the buffer of the base64 BIO
    BUF_MEM* bioBuffer;
    BIO_get_mem_ptr(base64Bio, &bioBuffer);
    char *result = (char *)malloc(bioBuffer->length+1);
    strcpy(result, bioBuffer->data);
    result[bioBuffer->length] = '\0';

    //Free the Base64 BIo and the associated memory BIO
    BIO_free_all(base64Bio);

    return result;
}

//////////////////////////////////////////
//msReadString
//Reads any available data in the connection structure
//////////////////////////////////////////

char*
msReadString(mscn* cn)
{
    //read socket data
    printf("Reading server message...");
    fflush(stdout);

    char* buffer = malloc(1024);
    int nread = -1;

    //If we use SSL, then read() must be done by the SSL library
    if(cn->usesSsl == 1)
    {
        nread = SSL_read(cn->sslConnection, buffer, 1024);
    }
        else
    {
        nread = read(cn->socket, buffer, 1024);
    }

    //read failed?
    if(nread == -1){
        printf("Fail (read())\n");
        return NULL;
    }

    //server aborted?
    else if(nread == 0){
        printf("Fail (connection closed)\n");
        return NULL;
    }

    //server wrote something?
    else{
        if(buffer[nread-2] == '\r');
            buffer[nread-2] = '\0';
        if( buffer[nread-1] == '\n')
            buffer[nread-1] = '\0';
        printf("Ok (recieved [%s])\n",buffer);
    }

    return buffer;
}

//////////////////////////////////////////
//msPrintSSLError
//Shows the equivalent string error when an SSL*
//function fails.
//////////////////////////////////////////

void
msPrintSSLError(SSL* sslConnection, int sslReturnCode)
{
    switch(SSL_get_error(sslConnection, sslReturnCode))
    {
        case SSL_ERROR_NONE:
        printf("SSL_ERROR_NONE\n");
        break;
        case SSL_ERROR_ZERO_RETURN:
        printf("SSL_ERROR_ZERO_RETURN\n");
        break;
        case SSL_ERROR_WANT_READ:
        printf("SSL_ERROR_WANT_READ\n");
        break;
        case SSL_ERROR_WANT_WRITE:
        printf("SSL_ERROR_WANT_WRITE\n");
        break;
        case SSL_ERROR_WANT_CONNECT:
        printf("SSL_ERROR_WANT_CONNECT\n");
        break;
        case SSL_ERROR_WANT_ACCEPT:
        printf("SSL_ERROR_WANT_ACCEPT\n");
        break;
        case SSL_ERROR_WANT_X509_LOOKUP:
        printf("SSL_ERROR_WANT_X509_LOOKUP\n");
        break;
        case SSL_ERROR_SYSCALL:
        printf("SSL_ERROR_SYSCALL\n");
        break;
        case SSL_ERROR_SSL:
        printf("SSL_ERROR_SSL\n");
        break;
    }
}

//////////////////////////////////////////
//msStrReplace
//Replaces characters by other in the given string
//////////////////////////////////////////

void
msStrReplace(char* strSource, char target, char replacement)
{
    int i;
    for(i=0; i<strlen(strSource); i++)
    {
        if(strSource[i] == target) strSource[i] = replacement;
    }
}

//////////////////////////////////////////

int main(int argc, char** argv)
{
//    mscn* cn = msConnect("smtp.gmail.com",587,1);
    mscn* cn = msConnect("smtp.live.com",587,1);
    msSendString("ehlo", cn);
    free(msReadString(cn));

    msSendString("auth login", cn);
    free(msReadString(cn));

    msSendString(msEncodeBase64(argv[1]), cn);
    free(msReadString(cn));

    msSendString(msEncodeBase64(argv[2]), cn);
    free(msReadString(cn));

//while(1)
//{
//    char buffer[1024];
//    fgets(buffer, 1024, stdin);
//    int i = strlen(buffer)-1;
//    if( buffer[i] == '\n')
//        buffer[i] = '\0';
//    msSendString(buffer,cn);
//    free(msReadString(cn));
//}


//    const char* source = "afalide@hotmail.fr\n";
//    char* encoded = NULL;
//    printf("Encoding string: [%s]\n",source);
//    encoded = msEncodeBase64(source);
//    printf("Result is:       [%s]\n",encoded);

    return 0;
}



/*
int msCreateSocket(int* fdSocket);
int msConnect(const char* url, int port, int fdSocket, int encrypted);
int msSendString(const char* str, int fdSocket);
int msReadString(char** str, int fdSocket);
void msPrintSSLError(SSL* sslConnection, int sslReturnCode);

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
    //Solve the url, we need an IP
    printf("Resolving %s...",url);
    struct hostent* host;
    host = gethostbyname(url);
    if(NULL == host)
    {
        printf("Fail (gethostbyname())\n");
        return 0;
    }

    //Many IP's may be found. We only take the first.
    struct in_addr**    solvedAddresses = (struct in_addr**) host->h_addr_list;
    struct in_addr*     solvedFirstAddress = solvedAddresses[0];
    struct sockaddr_in  server_sockaddr;
    socklen_t            server_sockaddr_size = sizeof(server_sockaddr);
    char*               ipPres = inet_ntoa(*solvedFirstAddress);
    printf("Ok (solved to %s)\n",ipPres);

    //Fill the server address structure with useful infos
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port   = htons((unsigned short) port);
    server_sockaddr.sin_addr = *solvedFirstAddress;


    //Connect to the server
    printf("Connecting to %s:%d...",ipPres,port);
    fflush(stdout);
    if(connect(fdSocket, (struct sockaddr*)&server_sockaddr, server_sockaddr_size) != 0)
    {
        printf("Fail\n");
        return 0;
    }
    printf("Ok\n");

    if(encrypted == 1)
    {
        //Here we use openSSL to encrypt the connection
        printf("Loading SSL...");
        fflush(stdout);

        //SSL Variables
        const SSL_METHOD*  sslMethod;
        SSL_CTX*            sslContext;
        SSL*                sslConnection;

        //Load the SSL library
        OpenSSL_add_all_algorithms();
        ERR_load_BIO_strings();
        ERR_load_crypto_strings();
        SSL_load_error_strings();

        if(SSL_library_init() < 0)
        {
            printf("Failed\n");
            return 0;
        }
        printf("Ok\n");

        //We will use TLS
        printf("Declaring TLSv1...");
        //sslMethod = SSLv23_client_method();
        sslMethod = TLSv1_client_method();
        if(NULL == sslMethod)
        {
            printf("Fail\n");
            return 0;
        }
        printf("Ok\n");

        //We create an SSL context (it will be associated to the socket)
        printf("Creating SSL context...");
        sslContext = SSL_CTX_new(sslMethod);
        if(NULL == sslContext)
        {
            printf("Fail\n");
            return 0;
        }
        printf("Ok\n");

        //This disables SSL2 for the context to force SSL3 negotiation
        SSL_CTX_set_options(sslContext, SSL_OP_NO_SSLv2);

        //We create an SSL structure
        printf("Creating connection structure...");
        sslConnection = SSL_new(sslContext);
        if(NULL == sslConnection)
        {
            printf("Fail\n");
            return 0;
        }
        printf("Ok\n");

        //This associates the SSL context with the socket
        SSL_set_fd(sslConnection, fdSocket);

        //Before using TLS we must issue a STARTTLS command to the smtp server
        char* buf = NULL;
        msReadString(&buf, fdSocket);
        //Send EHLO
        msSendString("EHLO\n",fdSocket);
        //Pump result
        msReadString(&buf, fdSocket);
        //Send STARTTLS
        msSendString("STARTTLS\n",fdSocket);
        msReadString(&buf, fdSocket);

        //Now we can connect with SSL
        printf("Establishing SSL session...");
        fflush(stdout);
        int sslConnectResult = SSL_connect(sslConnection);
        if(sslConnectResult != 1)
        {
            printf("Fail\n");
            msPrintSSLError(sslConnection, sslConnectResult);
            return 0;
        }
        printf("Ok\n");
    }

    return 1;
}

int msSendString(const char* str, int fdSocket)
{
    printf("Sending data [%s]...",str);
    fflush(stdout);

    int sent = write(fdSocket, str, strlen(str));

    if(sent == -1)
    {
        printf("Fail (write())\n");
        return 0;
    }else if (sent == 0){
        printf("Fail (0 bytes sent)\n");
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

    //server aborted?
    else if(nread == 0){
        printf("Fail (server ended connection)\n");
        return 0;
    }

    //server wrote something?
    else{
        //buffer[nread] = '\0';
        printf("Ok (recieved [%s])\n",buffer);
    }

    *str = buffer;
    return 1;
}

void msPrintSSLError(SSL* sslConnection, int sslReturnCode)
{
    switch(SSL_get_error(sslConnection, sslReturnCode))
    {
        case SSL_ERROR_NONE:
        printf("SSL_ERROR_NONE\n");
        break;
        case SSL_ERROR_ZERO_RETURN:
        printf("SSL_ERROR_ZERO_RETURN\n");
        break;
        case SSL_ERROR_WANT_READ:
        printf("SSL_ERROR_WANT_READ\n");
        break;
        case SSL_ERROR_WANT_WRITE:
        printf("SSL_ERROR_WANT_WRITE\n");
        break;
        case SSL_ERROR_WANT_CONNECT:
        printf("SSL_ERROR_WANT_CONNECT\n");
        break;
        case SSL_ERROR_WANT_ACCEPT:
        printf("SSL_ERROR_WANT_ACCEPT\n");
        break;
        case SSL_ERROR_WANT_X509_LOOKUP:
        printf("SSL_ERROR_WANT_X509_LOOKUP\n");
        break;
        case SSL_ERROR_SYSCALL:
        printf("SSL_ERROR_SYSCALL\n");
        break;
        case SSL_ERROR_SSL:
        printf("SSL_ERROR_SSL\n");
        break;
    }
}

int main()
{
    const char* url = "smtp.gmail.com";
    int port = 465;

//    const char* url = "www.google.com";
//    int port = 443;


    int serverSocket;
    char* buf = NULL;

    msCreateSocket(&serverSocket);
    msConnect(url,port,serverSocket,1);

    msSendString("MAIL FROM <toast@toast.com>\r\n",serverSocket);
    msReadString(&buf, serverSocket);

//    msSendString("EHLO\n", serverSocket);
//    msReadString(&buf, serverSocket);
//    msSendString("MAIL FROM <toast@toast.com>\n", serverSocket);
//    msReadString(&buf, serverSocket);
//    msSendString("STARTTLS\n", serverSocket);
//    msReadString(&buf, serverSocket);
//    msSendString("MAIL FROM abc.com\n", serverSocket);
//    msReadString(&buf, serverSocket);

//    if(!msCreateSocket(&serverSocket))
//    {
//        printf("Failed to create socket\n");
//        return 1;
//    }
//
//    if(!msConnect(url,port,serverSocket,1))
//    {
//        printf("Failed to connect\n");
//        return 1;
//    }
//
//    if(!msSendString("EHLO",serverSocket))
//    {
//        printf("Send failed\n");
//        return 1;
//    }
//
//    char* str = NULL;
//    if(!msReadString(&str, serverSocket))
//    {
//        printf("Nothing read\n");
//        return 1;
//    }
//    free(str);
//
//    if(!msSendString("MAIL FROM <toast@toast.com>",serverSocket))
//    {
//        printf("Send failed\n");
//        return 1;
//    }
//
//    if(!msReadString(&str, serverSocket))
//    {
//        printf("Nothing read\n");
//        return 1;
//    }

    return 0;
}
*/

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


