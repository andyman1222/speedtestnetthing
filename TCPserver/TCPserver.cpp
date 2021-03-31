/**
*
* THIS CODE HAS ORIGINATED AND BEEN MODIFIED FROM https://docs.microsoft.com/en-us/windows/win32/winsock/complete-server-code AND OTHER SOURCES
*
**/

#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <string>
#include <list>
#include <signal.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

using namespace std;

#define DEFAULT_BUFLEN 512
string TCPORT = "27015";

SOCKET ListenSocketTCP = INVALID_SOCKET;
SOCKET ClientSocketTCP = INVALID_SOCKET;

struct addrinfo* resultTCP = NULL;
struct addrinfo hintsTCP;

WSADATA wsaData;
int iResult;

std::list<HANDLE> con;
std::list<SOCKET> socks;

int cleanup(int r);
void cleanup();
void _cleanup(int r) {
    printf("Signal received: %d. Terminating program...\n", r);
    cleanup(r);
}

bool keepActive = true;

unsigned __stdcall ClientSession(void* data) {
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    int iResult_;
    int iSendResult;
    socks.push_back((SOCKET)data);
    SOCKET* ClientSocket = &socks.back();
    
    sockaddr_in clientInfo;
    int size = sizeof(clientInfo);
    getpeername(*ClientSocket, (struct sockaddr*)&clientInfo, &size);
    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientInfo.sin_addr, addr, sizeof(addr));
    printf("Client connected: %s\n", addr);
    do {

        //printf("Listening TCP...\n");
        iResult_ = recv(*ClientSocket, recvbuf, recvbuflen, 0);
        if (iResult_ > 0) {
            printf("Bytes TCP received: %d; message: \"%s\"; client: %s\n", iResult_, recvbuf, addr);

            // Echo the buffer back to the sender
            iSendResult = send(*ClientSocket, recvbuf, iResult_, 0);
            if (iSendResult == SOCKET_ERROR) {
                printf("send TCP failed with error: %d\n", WSAGetLastError());
                cleanup(1);
            }
            printf("Bytes TCP sent: %d; message: \"%s\"; client: %s\n", iResult_, recvbuf, addr);
            //do {
                //if (listenUDP) {

        }
        else if (iResult_ == 0) {
            printf("Connection TCP closing...\n");
                
        }

        else {
            printf("recv TCP failed with error: %d (Note: Client could've disconnected their end)\n", WSAGetLastError());

        }

    } while (iResult_ > 0 && keepActive);

    //close connection
    iResult = shutdown(*ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown TCP failed with error: %d\n", WSAGetLastError());
    }
    printf("Connection with %s terminated successfully.", addr);
    ExitThread(0);
}

int __cdecl main(void)
{
    atexit(cleanup);
    signal(SIGINT, _cleanup);
    signal(SIGTERM, _cleanup);
    signal(SIGSEGV, _cleanup);
    signal(SIGABRT, _cleanup);
    signal(SIGFPE, _cleanup);
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hintsTCP, sizeof(hintsTCP));
    hintsTCP.ai_family = AF_INET;
    hintsTCP.ai_socktype = SOCK_STREAM;
    hintsTCP.ai_protocol = IPPROTO_TCP;
    hintsTCP.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, TCPORT.c_str() , &hintsTCP, &resultTCP);
    if (iResult != 0) {
        printf("getaddrinfo TCP failed with error: %d\n", iResult);
        cleanup(1);
    }

    // Create a SOCKET for connecting to server
    ListenSocketTCP = socket(resultTCP->ai_family, resultTCP->ai_socktype, resultTCP->ai_protocol);
    if (ListenSocketTCP == INVALID_SOCKET) {
        printf("socket TCP failed with error: %ld\n", WSAGetLastError());
        cleanup(1);
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocketTCP, resultTCP->ai_addr, (int)resultTCP->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind TCP failed with error: %d\n", WSAGetLastError());
        cleanup(1);
    }

    freeaddrinfo(resultTCP);

    iResult = listen(ListenSocketTCP, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen TCP failed with error: %d\n", WSAGetLastError());
        cleanup(1);
    }

    printf("Listening for connections...\n");
    
    sockaddr_in clientInfo;
    int size = sizeof(clientInfo);
    char addr[INET_ADDRSTRLEN];
    
    // Accept a client socket
    while(keepActive){
        ClientSocketTCP = accept(ListenSocketTCP, (struct sockaddr*)&clientInfo, &size);
        inet_ntop(AF_INET, &clientInfo.sin_addr, addr, sizeof(addr));
        if (ClientSocketTCP == INVALID_SOCKET) {
            printf("accept TCP failed with error: %d\n", WSAGetLastError());
        }
        else {
            //printf("Client connected: %s\n", addr);
            con.push_back((HANDLE)_beginthreadex(NULL, 0, &ClientSession, (void*)ClientSocketTCP, 0, NULL));
        }
        
    }

    // cleanup
    cleanup(0);
}

void cleanup() {
    cleanup(0);
}

bool clean = false;
int cleanup(int code) {
    if (!clean) {
        clean = true;
        // shutdown the connection since no more data will be sent
        printf("Cleaning up... (Error messages may occur as sockets DC)\n");
        keepActive = false;

        if (ListenSocketTCP != INVALID_SOCKET) {

            closesocket(ListenSocketTCP);
        }

        if (ClientSocketTCP != INVALID_SOCKET) {
            for (SOCKET s : socks) {
                //close connection
                iResult = shutdown(s, SD_SEND);
                if (iResult == SOCKET_ERROR) {
                    printf("shutdown TCP failed with error: %d\n", WSAGetLastError());
                }
            }
            closesocket(ClientSocketTCP);
        }

        for (HANDLE h : con) {
            WaitForSingleObject(h, 1000); //wait time just in case for some reason socket isn't already closed
            if (!CloseHandle(h))
                printf("Thread termination failed. Error: %d", GetLastError());
        }

        if (resultTCP != NULL)
            freeaddrinfo(resultTCP);

        WSACleanup();
        exit(code);
    }

    
}