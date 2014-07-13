
#include "mysmtp.h"
#include "pwd.h"

#ifndef MYSMTP_BUFFER_LEN
#define MYSMTP_BUFFER_LEN 256
#endif

int main(int argc, char** argv)
{
    int     serverPort;
    mscn*   serverConnection;
    char*   serverIP;
    char    mailTo      [MYSMTP_BUFFER_LEN];
    char    mailFrom    [MYSMTP_BUFFER_LEN];
    char    subject     [MYSMTP_BUFFER_LEN];
    char    message     [MYSMTP_BUFFER_LEN];
    int     authenticationRequired = 0;
    //cstate  clientState;

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
        printf("Unable to connect to %s %d\n",serverIP,serverPort);
        return 1;
    }

    //Send EHLO
    msSendString("EHLO esgi_mysmtp_client",serverConnection);
    char* ehloResult = msReadString(serverConnection);
    if(NULL != strstr(ehloResult,"AUTH LOGIN"))
    {
        authenticationRequired = 1;
        mslog("An authentication will be required from the server\n");
    }
    free(ehloResult);

    //Input the FROM mail address
    printf("From:      ");
    fgets(mailFrom, sizeof(mailFrom), stdin);
    mailFrom[strlen(mailFrom)-1] = '\0';

    //Input the TO mail address
    printf("To:        ");
    fgets(mailTo, sizeof(mailTo), stdin);
    mailTo[strlen(mailTo)-1] = '\0';

    //The subject?
    printf("Subject:   ");
    fgets(subject, sizeof(subject), stdin);
    subject[strlen(subject)-1] = '\0';

    //Write a fine message here. The NSA wants to read it, so be polite.
    printf("Message:   ");
    fgets(message, sizeof(message), stdin);

    //Before sending the mail; authenticate if necessary
    if(1 == authenticationRequired)
    {
        msSendString("AUTH LOGIN",serverConnection);
        char* result = msReadString(serverConnection);
        if(! msStartsWith(result,"334"))
        {
            printf("Authentication unavailable\n");
            free(result);
            return 1;
        }
        free(result);

        char login [MYSMTP_BUFFER_LEN];
        char password [MYSMTP_BUFFER_LEN];

        //Input the login
        printf("Login:     ");
        fgets(login, MYSMTP_BUFFER_LEN, stdin);
        char* loginEnc = msEncodeBase64(login);
        msSendString(loginEnc,serverConnection);
        result = msReadString(serverConnection);
        if(! msStartsWith(result,"334"))
        {
            printf("Login failed\n");
            free(loginEnc);
            free(result);
            return 1;
        }
        free(loginEnc);
        free(result);

        //Input the password
        strncpy(password, getpass("Password:  "), MYSMTP_BUFFER_LEN);
        if(strlen(password) == 0)
        {
            printf("Password cannot be empty!\n");
            return 1;
        }
        char* passwordEnc = msEncodeBase64(password);
        msSendString(passwordEnc,serverConnection);
        result = msReadString(serverConnection);
        if(! msStartsWith(result,"235"))
        {
            printf("Login failed, incorrect username/password\n");
            free(passwordEnc);
            free(result);
            return 1;
        }
        free(result);
    }

    char* result = NULL;

    //Send the reverse path mail address
    char cmdMailFrom[MYSMTP_BUFFER_LEN];
    sprintf(cmdMailFrom,"MAIL FROM:<%s>",mailFrom);
    msSendString(cmdMailFrom,serverConnection);
    result = msReadString(serverConnection);
    if(! msStartsWith(result,"250"))
    {
        printf("Error: failed to set reverse path mail\n");
        free(result);
        return 1;
    }
    free(result);

    //Send the recipient address
    char cmdRcptTo[MYSMTP_BUFFER_LEN];
    sprintf(cmdRcptTo,"RCPT TO:<%s>",mailTo);
    msSendString(cmdRcptTo,serverConnection);
    result = msReadString(serverConnection);
    if(!msStartsWith(result,"250"))
    {
        printf("Error: failed to set recipent address\n");
        free(result);
        return 1;
    }

    //Send the mail data
    msSendString("DATA",serverConnection);
    free(msReadString(serverConnection));

    char cmdData[MYSMTP_BUFFER_LEN];
    sprintf(cmdData,"From: %s\r\nTo: %s\r\nSubject: %s\r\n%s",mailFrom,mailTo,subject,message);
    msSendString(cmdData,serverConnection);
    msSendStringNoCrlf("\r\n.\r\n",serverConnection);
    result = msReadString(serverConnection);

    return 0;
}





