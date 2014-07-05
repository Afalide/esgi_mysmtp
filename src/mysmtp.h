
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>

//////////////////////////////////////////
//The MySmtp logging function.
//It is a basic printf, but will not be
//compiled on release mode
//////////////////////////////////////////

#ifdef MYSMTP_DEBUG
#define mslog(format, ...) printf(format, ##__VA_ARGS__)
#else
#define mslog(format, ...)
#endif

//////////////////////////////////////////
//The MySmtp Connection structure
//////////////////////////////////////////

typedef struct mscn_s
{
    //Common data
    int                 socket;
    int                 usesSsl;
    char*               host;
    int                 port;

    //SSL data
    const SSL_METHOD*   sslMethod;
    SSL_CTX*            sslContext;
    SSL*                sslConnection;

    //Client SMTP data
    char*               cmd_ehlo;
    char*               cmd_rcptto;
    char*               cmd_mailfrom;
    char*               cmd_data;

} mscn;

//////////////////////////////////////////
//The clients structure
//////////////////////////////////////////

/*typedef struct msclient_s
{
    int         socket;

    char*       cmd_ehlo;
    char*       cmd_rcptto;
    char*       cmd_mailfrom;
    char*       cmd_data;
} msclient;*/

//////////////////////////////////////////
//MySmtp functions
//////////////////////////////////////////

mscn*           msConnect(const char* host, int port, int useSsl);
mscn*           msCatch(int socket, struct sockaddr_in* addr);
void            msSendString(const char* str, mscn* cn);
void            msSendStringNoCrlf(const char* str, mscn* cn);
char*           msEncodeBase64(const char* str);
char*           msReadString(mscn* cn);
void            msPrintSSLError(SSL* sslConnection, int sslReturnCode);
int             msStartsWith(const char* str, const char* searchFor);
char*           msGetParameter(const char* str, const char* searchFor);





