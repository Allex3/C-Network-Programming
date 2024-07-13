#include <winsock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <windows.h>

#define PORT "4200"
#define BACKLOG 10

// thread for sending stuff to client (beej uses processes, so he also closes the sockets and exits)
// idk how to use processes yet and idk why i'd have to
DWORD WINAPI sendToClient(LPVOID clSockfd) //LPVODI is void* 
{
    SOCKET* clsockcasted = (SOCKET*) clSockfd; //cast it to pointer of SOCKET
    if (send(*clsockcasted, "Hello, world!", 13, 0) == -1)
        fprintf(stderr, "send: %s\n", gai_strerror(WSAGetLastError()));

    closesocket(*clsockcasted); //we close the current client socket cuz we finished with them
    //if closed in main() it woudl close before we could send()
    //i guess this is why people use processes...
    //because they have the same connection mirrored so that risk is not possible
    return 0;
}

void *getInAddr(struct sockaddr *sa) // pointer for the IP Address (a struct containing an int), IPv4 or IPv6
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *)sa)->sin_addr);

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(void)
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
    SOCKET sockfd, clSockfd;
    int status;
    char yes = 1;
    struct addrinfo hints, *servinfo, *curraddr;
    struct sockaddr_storage clientAddr; // so it can store IPv4 and IPv6, can be cast to sockaddr
    char ipstr[INET6_ADDRSTRLEN];      // it's 65 so both IPv4 and IPv6 fit
    socklen_t sin_size; //some sort of unsigned int


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // both ipv4 and ipv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Fill in my IP for me

    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }
    for (curraddr = servinfo; curraddr != NULL; curraddr = curraddr->ai_next)
    {
        if ((sockfd = socket(curraddr->ai_family, curraddr->ai_socktype, curraddr->ai_protocol)) == -1)
        {
            fprintf(stderr, "server: socket: %s\n", gai_strerror(WSAGetLastError()));
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0)
        {
            fprintf(stderr, "setsockopt: %s\n", gai_strerror(WSAGetLastError()));
            exit(1);
        }

        if (bind(sockfd, curraddr->ai_addr, curraddr->ai_addrlen) != 0)
        {
            fprintf(stderr, "server: bind: %s\n", gai_strerror(WSAGetLastError()));
            continue;
        }

        break; //we only get the first successful one

    }

    char hostname[INET6_ADDRSTRLEN];
    gethostname(hostname, sizeof hostname);
    printf("this server's hostname is %s\n", hostname);
    
    if (curraddr == NULL) 
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    freeaddrinfo(servinfo); //we won't use this list anymore


    if (listen(sockfd, BACKLOG) == -1)
    {
        fprintf(stderr, "listen: %s\n", gai_strerror(WSAGetLastError()));
        exit(1);
    }
    
    printf("server: waiting for connections...");

    while (1) //main accept() loop
    {
        sin_size = sizeof clientAddr;
        
        clSockfd = accept(sockfd, (struct sockaddr*)&clientAddr, &sin_size);
        if (clSockfd == -1)
        {
            fprintf(stderr, "accept: %s\n", gai_strerror(WSAGetLastError()));
            continue; //look for another client
        } 
        
        //successfully accepted connection
        inet_ntop(clientAddr.ss_family, getInAddr((struct sockaddr*)&clientAddr), ipstr, sizeof ipstr);
        //now the IP is returned in string format in ipstr
        printf("server: got connection from %s\n", ipstr);  

        //now send message to client

        HANDLE svThread = CreateThread(NULL, 0, sendToClient, &clSockfd, 0, NULL);
        if (svThread == NULL)
        {
            fprintf(stderr, "thread");
            exit(1);
        }
        //if the thread works goes here, do not wait for it, let it run 
    }

    return 0;
}