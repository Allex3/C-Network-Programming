#define _WIN32_WINNT 0x600


#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }
    if (LOBYTE(wsaData.wVersion) != 2 ||
        HIBYTE(wsaData.wVersion) != 2)
    {
        fprintf(stderr, "Versiion 2.2 of Winsock is not available.\n");
        WSACleanup();
        exit(2);
    }

    // getaddrinfo has 4 parameters - node = domain/id, service=port, hints=struct addrinfo
    // the 4th one is for the results, we just declare it
    // the last two arguments take the address of said struct variables
    // addrinfo - has the type of IP address, type of socket stream, and how to fill IP

    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo; // will point to the results - linked list

    // example code to listen (server) on my network IP address, with the port 4200 (the client)
    memset(&hints, 0, sizeof hints); // make sure the struct is empty //&hints = address of the variable hints
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets (TCP is used for transmitting data)
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    if ((status = getaddrinfo(NULL, "4200", &hints, &servinfo) != 0))
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status)); // type of error we get
        // status should be 0 if all is fine and running
        exit(1); // exit with error code 1 for FAILURE
    }

    freeaddrinfo(servinfo); // free the linked-list

// now an example to connect the client to a specific server - here www.example.com, port 4200

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    // get ready to connect
    status = getaddrinfo("www.example.com", "4200", &hints, &servinfo);

    // servinfo now points to a linked list of 1 or more struct addrinfos

    freeaddrinfo(servinfo);
    WSACleanup();
    return 0;
}