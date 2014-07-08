
#include "mysmtp.h"

#ifndef MYSMTP_BUFFER_LEN
#define MYSMTP_BUFFER_LEN 256
#endif

int main(int argc, char** argv)
{
    int     serverPort;
    char*   serverIP;
    mscn*   serverConnection;
    char    mailTo      [MYSMTP_BUFFER_LEN];
    char    mailFrom    [MYSMTP_BUFFER_LEN];
    char    subject     [MYSMTP_BUFFER_LEN];
    char    message     [MYSMTP_BUFFER_LEN];

    //A server connection is required before sending anything:
    if(argc <3)
    {
        printf("Wrong number of parameters\n");
        return 1;
    }

    //Read the port
    serverPort = (int) strtol(argv[2],NULL,10);
    if(0 == serverPort)
    {
        printf("Port value (%s) is wrong\n",argv[2]);
        return 1;
    }

    //Read the IP
    serverIP = argv[1];

    //Check if we use SSL
    int ssl = 0;
    if(argc == 4 && 0==strcmp(argv[3], "ssl"))
        ssl = 1;

    //Connect to the smtp relay
    serverConnection = msConnect(serverIP,serverPort,ssl);
    if(NULL == serverConnection)
    {
        printf("Unable to connect\n");
        return 1;
    }

    //Send EHLO
    msSendString("EHLO esgi_mysmtp_client",serverConnection);
    free(msReadString(serverConnection));


    printf("Welcome to the MySmtp client!\n");

    //Input the target mail address
    printf("From:      ");
    fgets(mailTo, sizeof(mailTo), stdin);

    //Input the FROM address
    printf("To:        ");
    fgets(mailFrom, sizeof(mailFrom), stdin);

    //The subject?
    printf("Subject:   ");
    fgets(subject, sizeof(subject), stdin);

    //Write a fine message here. The NSA wants to read it, so be polite.
    printf("Message:   ");
    fgets(message, sizeof(message), stdin);


//    //From now on, the client MAY be interrupted if the
//    //server goes offline. This is done by the
//    //select function
//    fd_set set;
//	FD_ZERO(&set);
//
//    while(1)
//    {
//        mslog("Calling select...");
//        FD_ZERO(&set);
//        FD_SET(serverConnection->socket, &set);
//        FD_SET(STDIN_FILENO, &set);
//        result = select(max(serverConnection->socket, STDIN_FILENO)+1, &set, NULL, NULL, NULL);
//
//        //Select has returned. Check where the event is
//
//        //stdin event?
//        if(FD_ISSET(STDIN_FILENO, &set))
//        {
//
//        }
//
//        //server socket event?
//        if(FD_ISSET(serverConnection->socket, &set))
//        {
//
//        }
//    }





//    char* res1 = msGetParameter("MAIL FROM:   <plop@mail.com> .", "MAIL");
//    char* res2 = msGetParameter(res1,"FROM:");
//
//    printf("%s\n",res1 ? res1 : "null");
//    printf("%s\n",res2 ? res2 : "null");
//
//    free(res1);
//    free(res2);

//    printf("%s\n",);

    //printf("%d\n", msStartsWith(argv[1], "EHLO"));
//    printf("%s\n",msGetParameter());

//    mscn* cn = msConnect("smtp.gmail.com",587,1);
////    mscn* cn = msConnect("smtp.live.com",587,1);
//    msSendString("ehlo", cn);
//    free(msReadString(cn));
//
//    msSendString("auth login", cn);
//    free(msReadString(cn));
//
//    msSendString(msEncodeBase64(argv[1]), cn);
//    free(msReadString(cn));
//
//    msSendString(msEncodeBase64(argv[2]), cn);
//    free(msReadString(cn));

//    const char* source = "romain.notari@gmail.com";
//    char* encoded = NULL;
//    printf("Encoding string: [%s]\n",source);
//    encoded = msEncodeBase64(source);
//    printf("Result is:       [%s]\n",encoded);

    return 0;
}









