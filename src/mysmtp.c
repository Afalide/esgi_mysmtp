
#include "mysmtp.h"

//////////////////////////////////////////
//msConnect
//Creates an mscn structure, which is used by the ms* functions.
//The cn object will be connected to the specified server
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
    ret->cmd_data = NULL;
    ret->cmd_data_isComposing = 0;
    ret->cmd_ehlo = NULL;
    ret->cmd_mailfrom = NULL;
    ret->cmd_rcptto = NULL;

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
        msSendString("EHLO",ret);
        //Flush again the response
        free(msReadString(ret));
        //Send STARTTLS
        msSendString("STARTTLS",ret);
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
    else
    {
        //We simply send a basic EHLO, so both ssl and no-ssl connections
        //are returned with an EHLO sent.

        //Flush the server welcome message
        free(msReadString(ret));
        //Make an EHLO
        msSendString("EHLO",ret);
        free(msReadString(ret));
    }

    return ret;
}

//////////////////////////////////////////
//msCatch
//Creates an mscn structure, based on an existing socket
//and sockaddr.
//////////////////////////////////////////

mscn*
msCatch(int socket, struct sockaddr_in* addr)
{
    if(0 == socket)
    {
        mslog("socket is 0, cannot catch it\n");
        return NULL;
    }

    if(NULL == addr)
    {
        mslog("sockaddr struct is NULL, cannot catch it\n");
        return NULL;
    }

    mscn* cn = (mscn*) malloc (sizeof(mscn));

    //Put everything to NULL
    cn->cmd_data = NULL;
    cn->cmd_data_isComposing = 0;
    cn->cmd_ehlo = NULL;
    cn->cmd_mailfrom = NULL;
    cn->cmd_rcptto = NULL;
    cn->sslConnection = NULL;
    cn->sslContext = NULL;
    cn->sslMethod = NULL;
    cn->usesSsl = 0;

    cn->socket = socket;

    //Retrieve the IP (presentation form, like '192.168.0.1')
    cn->host = (char*) malloc (16);
    inet_ntop(AF_INET, &addr->sin_addr, cn->host, 16);
    cn->host[15] = '\0';
    mslog("Retrieved IP:   %s\n", cn->host);

    //Retrieve the port
    cn->port = addr->sin_port;
    mslog("Retrieved Port: %d\n", cn->port);

    return cn;
}

//////////////////////////////////////////
//msSendString
//Sends a single string to an host
//////////////////////////////////////////

void
msSendString(const char* str, mscn* cn)
{
    if(!str || !cn)
    {
        mslog("NULL parameter(s): nothing sent\n");
        return;
    }

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

    free(strWithCrlf);

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
//msSendStringNoCrlf
//Sends a single string to an host, but doesn't
//append a CRLF to the end of the string.
//This is useful to send a string with
//separated calls
//////////////////////////////////////////

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
    int bufferLen = bioBuffer->length;
    char *result = (char *)malloc(bufferLen);

    strcpy(result, bioBuffer->data);
    result[bufferLen-1] = '\0';

    //Free the Base64 BIO and the associated memory BIO
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
    mslog("Reading server message...");
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
        nread = read(cn->socket, buffer, 1024 +1);
    }

    //read failed?
    if(nread == -1){
        mslog("Fail (read())\n");
        return NULL;
    }

    //client aborted?
    else if(nread == 0){
        mslog("Fail (connection closed)\n");
        return NULL;
    }

    //client wrote something?
    else{
//        if( buffer[nread-2] == '\r');
//            buffer[nread-2] = '\0';
//        if( buffer[nread-1] == '\n')
//            buffer[nread-1] = '\0';
        //We must add a proper CRLF to the end of the buffer
        buffer[nread] = '\0';
        mslog("Ok, recieved [%s] [size %d]\n",buffer,nread);
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
//msStartsWith
//Returns 1 if target is found at the beginning
//of str
//////////////////////////////////////////

int
msStartsWith(const char* str, const char* target)
{
    if(NULL == str || NULL == target)
        return 0;

    int i, lenSource, lenTarget;

    lenSource = strlen(str);
    lenTarget = strlen(target);

    if(lenSource < lenTarget)
        return 0;

    for(i=0; i<lenTarget; i++)
    {
        if(str[i] != target[i])
            return 0;
    }

    return 1;
}

//////////////////////////////////////////
//msEndsWith
//Returns 1 if target is found at the end
//of str
//////////////////////////////////////////

int msEndsWith(const char* str, const char* target)
{
    if(NULL == str || NULL == target)
        return 0;

    int i, j, lenSource, lenTarget;

    lenSource = strlen(str);
    lenTarget = strlen(target);

    if(lenSource < lenTarget)
        return 0;

    i=lenSource-1;
    j=lenTarget-1;

    while(j>=0)
    {
        //mslog("Comparing %c and %c\n",str[i],target[j]);
        if(str[i] != target[j] && str[i])
            return 0;
        --i;
        --j;
    }

    return 1;
}

//////////////////////////////////////////
//msGetParameter
//Returns the parameter of a command in a given string.
//ie: msGetParameter("auth LOGIN","auth") returns "LOGIN"
//////////////////////////////////////////

char*
msGetParameter(const char* str, const char* searchFor)
{
    if(!str || !searchFor)
        return NULL;

    if(!msStartsWith(str,searchFor))
    {
        printf("%s not starting with %s\n",str, searchFor);
        return NULL;
    }

    int lenStr;
    int lenSearch;
    int lenParameter;
    int i;
    char* result = NULL;

    lenStr = strlen(str);
    lenSearch = strlen(searchFor);

    //Skip the command name
    i = lenSearch;

    //Skip space(s) between the command and the parameter
    while(str[i] == ' ' && i<lenStr)
        i++;

    //If we are ath the end of the full string, it means
    //there is no parameter
    if(i >= lenStr)
        return NULL;

    //Now we are at the start of the parameter string
    lenParameter = lenStr - i;
    result = (char*) malloc(lenParameter +1);

    //Copy the parameter to the result string
    int j = 0;
    for(;i<lenStr; i++)
        result[j++] = str[i];

    return result;
}

