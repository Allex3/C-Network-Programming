#include <winsock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <windows.h>

#define PORT "4200"
#define BACKLOG 10
#define MAXDATASIZE 10000
const int NOTFOUND = 0x3f3f3f3f; // made it infinity because you can't look for infinity here (anything beyond the scope of my server)

void startWSA();
SOCKET constructServer();
void lookForConnections(SOCKET *svsockp);
void *getInAddr(struct sockaddr *sa);
DWORD WINAPI handleClient(void *clsockp);
int getClientRequest(SOCKET clientFD, char *readBuffer);

// functions for checking the client request
void checkNothing(SOCKET clientFD, char *reqPath, int *bytesSent);
void checkNotFound(SOCKET clientFD, char *reqPath, int *bytesSent);
void checkEcho(SOCKET clientFD, char *reqPath, int *bytesSent);
void checkUserAgent(SOCKET clientFD, char *reqPath, char *readBuffer, int *bytesSent);

// functions to POST or GET to/from the client
void postFile(SOCKET clientFD, char *reqPath, char *readBuffer, int *bytesSent, char *method);
void getFile(SOCKET clientFD, char *reqPath, char *readBuffer, int *bytesSent, char *method);

int main(int argc, char *argv[])
{
    startWSA();

    SOCKET serverFD = constructServer();

    if (listen(serverFD, BACKLOG) == -1)
    {
        fprintf(stderr, "listen: %s\n", gai_strerror(WSAGetLastError()));
        exit(1);
    }

    lookForConnections(&serverFD);

    return 0;
}

void startWSA()
{
    // check the required winsock version and try to start it
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
}
void *getInAddr(struct sockaddr *sa) // poin    ter for the IP Address (a struct containing an int), IPv4 or IPv6
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *)sa)->sin_addr);

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

SOCKET constructServer()
{
    SOCKET serverFD;
    int status;
    char yes = 1;
    struct addrinfo hints, *servinfo, *curraddr;

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
        if ((serverFD = socket(curraddr->ai_family, curraddr->ai_socktype, curraddr->ai_protocol)) == -1)
        {
            fprintf(stderr, "server: socket: %s\n", gai_strerror(WSAGetLastError()));
            continue;
        }

        if (setsockopt(serverFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0)
        {
            fprintf(stderr, "setsockopt: %s\n", gai_strerror(WSAGetLastError()));
            exit(1);
        }

        if (bind(serverFD, curraddr->ai_addr, curraddr->ai_addrlen) != 0)
        {
            fprintf(stderr, "server: bind: %s\n", gai_strerror(WSAGetLastError()));
            continue;
        }

        break; // we only get the first successful one
    }

    if (curraddr == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    freeaddrinfo(servinfo); // we won't use this list anymore

    return serverFD;
}

void lookForConnections(SOCKET *svsockp)
{
    printf("server: WAITING FOR CONNECTIONS...\n");

    SOCKET serverFD = *svsockp;
    socklen_t sin_size;                 // size of client address
    struct sockaddr_storage clientAddr; // client ipv4 or ipv6
    char ipstr[INET6_ADDRSTRLEN];       // it's 65 bytes so both IPv4 and IPv6 fit
    SOCKET clsockfd;
    while (1)
    {
        sin_size = sizeof clientAddr;

        clsockfd = accept(serverFD, (struct sockaddr *)&clientAddr, &sin_size);
        if (clsockfd == -1)
            continue; // search for clients more

        // now handle the client with a thread for them

        // successfully accepted connection
        inet_ntop(clientAddr.ss_family, getInAddr((struct sockaddr *)&clientAddr), ipstr, sizeof ipstr);
        // now the IP is returned in string format in ipstr
        printf("server: got connection from %s\n", ipstr);

        HANDLE clThread = CreateThread(NULL, 0, handleClient, &clsockfd, 0, NULL);
        if (clThread == NULL)
        {
            fprintf(stderr, "thread");
            exit(1);
        }
    }
}

DWORD WINAPI handleClient(void *clsockp)
{
    SOCKET clientFD = *((SOCKET *)clsockp);
    char readBuffer[MAXDATASIZE]; // readBuffer to receive the data from client (link of the page i think here)

    if (getClientRequest(clientFD, readBuffer) != 0)
        return 1;

    char *method = strdup(readBuffer);

    printf("Content:\n %s\n", readBuffer);
    method = strtok(method, " "); // GET POST etc, the first word of the readBuffer content it is
    printf("Method: %s\n", method);

    char *reqPath = strdup(readBuffer); // make a copy for the buffer where we store the reqPath /../..
    reqPath = strtok(reqPath, " ");     // GET /... HTTP/1.1, so this gets the first word before a space, GET
    reqPath = strtok(NULL, " ");        // the second word before a space after the first space, so the /... page, and check it

    // we want to go in / only, so check that

    int bytesSent = NOTFOUND; // if the bytesSent remain NOTFOUND, a value >0 but above all the possible values
    // but not <0 so not an error, it means the page we're looking for isn't found

    checkNothing(clientFD, reqPath, &bytesSent);
    checkEcho(clientFD, reqPath, &bytesSent);
    checkUserAgent(clientFD, reqPath, readBuffer, &bytesSent);

    postFile(clientFD, reqPath, readBuffer, &bytesSent, method);
    getFile(clientFD, reqPath, readBuffer, &bytesSent, method);

    checkNotFound(clientFD, reqPath, &bytesSent);

    if (bytesSent < 0)
    {
        fprintf(stderr, "sent: %s\n", gai_strerror(WSAGetLastError()));
        return 1;
    }

    Sleep(1);
    closesocket(clientFD);
    return 0;
}

int getClientRequest(SOCKET clientFD, char *readBuffer)
{
    // receive request data from client and stores it into the buffer
    int bytesRecv = recv(clientFD, readBuffer, MAXDATASIZE, 0);
    if (bytesRecv < 0)
    {
        fprintf(stderr, "recv: %s\n", gai_strerror(WSAGetLastError()));
        closesocket(clientFD);
        return 1;
    }
    if (bytesRecv == 0) // client forcefully closed the connection
    {
        fprintf(stderr, "Client disconnected unexpectedly.\n");
        return 2;
    }
    // received well
    readBuffer[bytesRecv] = '\0';

    return 0;
}

void checkNothing(SOCKET clientFD, char *reqPath, int *bytesSent)
{
    if (strcmp(reqPath, "/") != 0)
        return;

    char *ok200 = "HTTP/1.1 200 OK";
    *bytesSent = send(clientFD, ok200, strlen(ok200), 0);
}

void checkNotFound(SOCKET clientFD, char *reqPath, int *bytesSent)
{
    if ((*bytesSent) == NOTFOUND)
    {
        char *notFound = "HTTP/1.1 404 NOT FOUND";
        *bytesSent = send(clientFD, notFound, strlen(notFound), 0);
    }
}

void checkEcho(SOCKET clientFD, char *reqPath, int *bytesSent)
{
    if (strncmp(reqPath, "/echo/", 6) != 0) // the copy of /../ if the first characters are /echo then /..., strcmp returns 0
        return;

    char res[MAXDATASIZE];

    char *reqPathCopy = strdup(reqPath); // make a copy of the /../.. path string to use with strtok
    // tbh i don't understand why doing strtok(reqPath, "/") doesn't wrok but it doesn't, it goes straight to HTTP

    char *content = strtok(reqPathCopy, "/"); // get the content after the first /
    content = strtok(NULL, "/");              // get the content after second /, which is what we want to show on screen

    sprintf(res, "HTTP/1.1 200 OK\nContent-Type: text/html\n\n<b>%s</b>", content); // print to string for putting the variable here easier
    printf("Sending response: %s\n", res);
    *bytesSent = send(clientFD, res, strlen(res), 0);
}

void checkUserAgent(SOCKET clientFD, char *reqPath, char *readBuffer, int *bytesSent)
{
    if (strcmp(reqPath, "/user-agent") != 0)
        return;

    char res[MAXDATASIZE];

    char *reqPathCopy = strdup(readBuffer); // make a copy of the buffer read from the client and search for User-Agent in it
    // using strtok, so that the original buffer isn't modified

    char *body = strtok(strstr(reqPathCopy, "User-Agent"), "\n"); // find User-Agent in the buffer we have, sort
    // and then stop when founding a newline after it so we only show that line
    int contentLength = strlen(body);

    sprintf(res, "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: %d\n\n<b>%s</b>", contentLength, body);
    printf("Sending response: %s\n", res);
    *bytesSent = send(clientFD, res, strlen(res), 0);
}

// POSTs to a file on the server, basically adds content to an existing file on the server from the client
// while GET just gets (reads) a file on the server and shows it on the client
void postFile(SOCKET clientFD, char *reqPath, char *readBuffer, int *bytesSent, char *method)
{
    if (!(strncmp(reqPath, "/files/", 7) == 0 && strcmp(method, "POST") == 0))
        return;

    // parse the filename from the path

    char *reqPathCopy = strdup(reqPath);
    char *filename = strtok(reqPathCopy, "/"); // files\0example.html
    filename = strtok(NULL, "");               // get into example.html, because the / after files/ became \0, we need no separators to get only after it

    // Get the contents of the POST request
    // I will make this POST request with curl -vvv -d "some data" localhost:4200/files/...
    // but generally it should work with any POST request

    char *reqCopy = strdup(readBuffer);              // use the content of the HTTP Request 
    char *content = strstr(reqCopy, "Content-Type"); // ContentType: application/x-www-form-urlencoded
    // below this with one more empty row is the actual content we want

    // more parsing..
    int i = 0;
    while (content[i++] != '\n')
        ; // stops at \n, now check for " " and "\n" until we reach the content we want to show
    while (strchr(" \n", content[i++]) != NULL)
        ;
    // now we reached an actual character that's not sapce or \n
    // IMPORTANT: So for some reason content[i] here is '\n' fuck knows why ima do i++, it may be because it finds \r\n ? idk
    i++;
    content = &(content[i]);
    int contentLength = strlen(content);

    // create/modify a file
    printf("\n-------\nCreate a file %s with content length: %d\n\n%s\n-------\n", filename, contentLength, content);

    // Open the file in write binary mode
    //NOTE: wb or rb instead of w and r, means translations are usually avoided
    //like in my case on Windows, it doesn't trasnlate \n in \r\n like it does when I use C
    FILE *fp = fopen(filename, "wb");
    if (!fp) // can't be opened/created
    {
        fprintf(stderr, "File could not be opened\n");
        char *res = "HTTP/1.1 404 Not Found\n\n"; // HTTP Response
        *bytesSent = send(clientFD, res, strlen(res), 0);
        return;
    }

    printf("Opening file %s\n", filename);

    // Write the contents to file
    if (fwrite(content, 1, contentLength, fp) != contentLength)
    {
        // it wrote less than intended
        fprintf(stderr, "Error writing the data to file\n");
        return;
    }

    fclose(fp); // close the file

    // Return contents
    char response[MAXDATASIZE];
    sprintf(response, "HTTP/1.1 201 Created\nContent-Type: application/octet-stream\nContent-Length: %d\n\n%s\n", contentLength, content);
    *bytesSent = send(clientFD, response, strlen(response), 0);

    fflush(stdout);
}

void getFile(SOCKET clientFD, char *reqPath, char *readBuffer, int *bytesSent, char *method)
{
    if (!(strncmp(reqPath, "/files/", 7) == 0 && strcmp(method, "GET") == 0))
        return;

    // parse the filename from the path

    char *reqPathCopy = strdup(reqPath);
    char *filename = strtok(reqPathCopy, "/"); // files\0example.html
    filename = strtok(NULL, "");               // get into example.html,
    
    // Open the file in reading mode and check if it exists
    FILE *fp = fopen(filename, "rb");
    if (!fp) // can't be opened/created
    {
        fprintf(stderr, "File could not be opened\n");
        char *res = "HTTP/1.1 404 Not Found\n\n"; // HTTP Response
        *bytesSent = send(clientFD, res, strlen(res), 0);
        return;
    }

    printf("Opening file %s\n", filename);

    //use fseek to get the cursor to the end of the file
    if (fseek(fp, 0, SEEK_END) < 0)
    {
        fprintf(stderr, "Error reading the file: fseek\n");
        return;
    }

    size_t dataSize = ftell(fp); //how many bytes are there until the end of the file (that's what we used fseek for)
    //this is the number of bytes we read

    //Rewind the cursor back to the beginning of file
    rewind(fp); 

    //Allocate memory for the contents of the file
    void *data =malloc(dataSize); 

    //Try to read data one byte at a time and check if all of it can be read
    if (fread(data, 1, dataSize, fp) != dataSize)
    {
        fprintf(stderr, "Error reading the file: fread\n");
        return;
    }

    //and that's it..
    char response[MAXDATASIZE];
    sprintf(response, "HTTP/1.1 200 OK\nContent-Length: %d\nContent-Type: text/html\n\n<b>%s</b>", dataSize, data);
    *bytesSent = send(clientFD, response, strlen(response), 0);

}
