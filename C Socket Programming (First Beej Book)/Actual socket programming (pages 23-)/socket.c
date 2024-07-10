#include <winsock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    struct addrinfo hints, *res;
    int status, s;

    if (argc != 2)
    {
        fprintf(stderr, "usage: socket hostname\n");
        return 1;
    }

    // setup hints - hints about the socket we want to connect to
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    if (status = getaddrinfo(NULL, "http", &hints, &res) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    printf("IP addresses for %s:\n\n", argv[1]);

    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) // run through res linked list
    {
        void *addr; // declares a pointer without a specific data type, will hold address of a struct in_addr
        // in_addr is a struct that only has a 32bit integer and represents an IP Address

        char *ipver;

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        // p->ai_addr is of struct sockaddr, but it can be cast to sockaddr_in because of the padding, so it's easier to work with
        if (p->ai_family == AF_INET)
        { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            // so here p->ai_addr accesses the pointer to the sockaddr struct ai_addr
            // so we change it to sockaddr_in pointer explicitly cuz p->ai_addr is just sockaddr, not sockaddr_in
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        }
        else
        { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }
        // convert the IP to a string and print it:

        char ipstr[INET6_ADDRSTRLEN];                       // the length of the string is the longest form an ip address can take INET6_ADDRSTRLEN
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr); // network to presentation
        printf(" %s: %s\n", ipver, ipstr);

        // now let's work a bit with sockets
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        // p->ai_family even if it's AF_INET it can be passed for PF_INET because they're the same usually
        // ai_socktype is stream or datagram (SOCK_STREAM or SOCK_DGRAM)
        // ai_protocol is 0 for any, it can be tcp/udp, etc.

        if (s == -1) // error
        {
            fprintf(stderr, "Failure getting a socket\n");
            return 3;
        }
    }

    freeaddrinfo(res);

    WSACleanup();
    return 0;
}