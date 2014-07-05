
#include "mysmtp.h"

int main(int argc, char** argv)
{
    int     serverPort;
    char*   serverIP;
    mscn*   serverConnection;

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

    //Connect to the smtp relay
    serverConnection = msConnect(serverIP,serverPort,0);
    if(NULL == serverConnection)
    {
        printf("Unable to connect\n");
        return 1;
    }

    msReadString(serverConnection);



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









