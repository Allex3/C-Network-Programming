#include <winsock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <windows.h>

#define PORT "4200"     // the port client will be connecting to
#define MAXDATASIZE 200 // max number of bytes we can get at once

void *getInAddr(struct sockaddr *sa);

DWORD WINAPI recvFromServer(LPVOID svsockvoid);

int main(int argc, char *argv[])
{

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }
    // LOBYTE is for the 2 after ., and HIBYTE before . , combposing 2.2 in the version
    if (LOBYTE(wsaData.wVersion) != 2 ||
        HIBYTE(wsaData.wVersion) != 2)
    {
        fprintf(stderr, "Versiion 2.2 of Winsock is not available.\n");
        WSACleanup();
        exit(2);
    }

    SOCKET sockfd, numBytesSend, status;
    struct addrinfo hints, *servInfo, *servAddr;
    char ipstr[INET6_ADDRSTRLEN];

    if (argc != 2)
    {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(argv[1], PORT, &hints, &servInfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: ", gai_strerror(status));
        return 1;
    }

    for (servAddr = servInfo; servAddr != NULL; servAddr = servAddr->ai_next)
    {
        if ((sockfd = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol)) == -1)
        { // ipV4 or ipv6, sock stream here, and the protocol we defined
            perror("client: socket");
            continue; // try to find another address
        }

        if (connect(sockfd, servAddr->ai_addr, servAddr->ai_addrlen) != 0)
        {
            perror("client: connect");
            continue; // try another
        }

        // found one that works
        break;
    }

    if (servAddr == NULL)
    {
        fprintf(stderr, "client: failed to connect");
        return 2;
    }

    inet_ntop(servAddr->ai_family, getInAddr(servAddr->ai_addr), ipstr, sizeof(ipstr));
    printf("client: connecting to %s\n", ipstr);

    freeaddrinfo(servInfo); // we won't usei t anymore

    //run a while loop to always check for input to send to the server and to other clients on the chat
    HANDLE inputThread = CreateThread(NULL, 0, recvFromServer, &sockfd, 0, NULL);
    if (inputThread == NULL)
    {
        fprintf(stderr, "thread");
        exit(1);
    }

    char clientInput[MAXDATASIZE];
    while (1)
    {
        fgets(clientInput, sizeof clientInput, stdin);
        //NOTE: FOR SOME REASON STRLEN() OF IT RETURNS 1 AFTER \0, AS IF IT'S \n COUNTED TOO ??
        if ((numBytesSend = send(sockfd, clientInput, strlen(clientInput)-1, 0)) == -1)
        {
            fprintf(stderr, "send: %s\n", gai_strerror(WSAGetLastError()));
            exit(1);
        } //because we always "wait" for input with fgets(), exit shouldnt be an issue because it wont be an error
        //because we send everytime something, and if its error its real and should exit
    }

    closesocket(sockfd);

    return 0;
}

void *getInAddr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *)sa)->sin_addr);

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

DWORD WINAPI recvFromServer(LPVOID svsockvoid)
{
    SOCKET *sockfdp = (SOCKET*) svsockvoid;
    SOCKET sockfd = *sockfdp; //the server socket
    int numBytesRecv; //bytes of message
    char buffer[MAXDATASIZE]; //for message recieved
    char fromClientNr;

    while (1)
    {
        if ((numBytesRecv = recv(sockfd, buffer, MAXDATASIZE - 1, 0)) == -1)
        {
            if (WSAGetLastError() == 10054) // forced close (only type of close yet lmao)
                {
                    closesocket(sockfd);
                    return 0;
                }
            fprintf(stderr, "recv: %s\n", gai_strerror(WSAGetLastError()));
            continue; //until we get a message
        }
        fromClientNr = buffer[numBytesRecv - 1]; //here is the number of client the way I coded it lmao
        buffer[numBytesRecv - 1] = '\0'; //end the string
        printf("client %c: %s\n", fromClientNr, buffer);
        fflush(stdout);
    }
    closesocket(sockfd);
    return 0;
}