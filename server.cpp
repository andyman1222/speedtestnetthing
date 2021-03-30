#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define TCPORT "27015"
#define UDPORT "27015"

SOCKET ListenSocketTCP = INVALID_SOCKET;
SOCKET ClientSocketTCP = INVALID_SOCKET;
SOCKET ListenSocketUDP = INVALID_SOCKET;
SOCKET ClientSocketUDP = INVALID_SOCKET;

struct addrinfo *resultTCP = NULL, *resultUDP = NULL;
struct addrinfo hintsTCP, hintsUDP;

int cleanup(int r);
void cleanup();


int __cdecl main(void)
{
    atexit(cleanup);
    WSADATA wsaData;
    int iResult;

    

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

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

    ZeroMemory(&hintsUDP, sizeof(hintsUDP));
    hintsUDP.ai_family = AF_INET;
    hintsUDP.ai_socktype = SOCK_DGRAM;
    hintsUDP.ai_protocol = IPPROTO_UDP;
    hintsUDP.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, TCPORT, &hintsTCP, &resultTCP);
    if (iResult != 0) {
        printf("getaddrinfo TCP failed with error: %d\n", iResult);
        cleanup(1);
    }

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, UDPORT, &hintsUDP, &resultUDP);
    if (iResult != 0) {
        printf("getaddrinfo UDP failed with error: %d\n", iResult);
        cleanup(1);
    }

    // Create a SOCKET for connecting to server
    ListenSocketTCP = socket(resultTCP->ai_family, resultTCP->ai_socktype, resultTCP->ai_protocol);
    if (ListenSocketTCP == INVALID_SOCKET) {
        printf("socket TCP failed with error: %ld\n", WSAGetLastError());
        cleanup(1);
    }

    // Create a SOCKET for connecting to server
    ListenSocketUDP = socket(resultUDP->ai_family, resultUDP->ai_socktype, resultUDP->ai_protocol);
    if (ListenSocketUDP == INVALID_SOCKET) {
        printf("socket UDP failed with error: %ld\n", WSAGetLastError());
        cleanup(1);
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocketTCP, resultUDP->ai_addr, (int)resultTCP->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind TCP failed with error: %d\n", WSAGetLastError());
        cleanup(1);
    }

    // Setup the UDP listening socket
    iResult = bind(ListenSocketUDP, resultUDP->ai_addr, (int)resultUDP->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind UDP failed with error: %d\n", WSAGetLastError());
        cleanup(1);
    }

    freeaddrinfo(resultTCP);
    freeaddrinfo(resultUDP);

    iResult = listen(ListenSocketTCP, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen TCP failed with error: %d\n", WSAGetLastError());
        cleanup(1);
    }

    iResult = listen(ListenSocketUDP, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen UDP failed with error: %d\n", WSAGetLastError());
        cleanup(1);
    }

    // Accept a client socket
    ClientSocketTCP = accept(ListenSocketTCP, NULL, NULL);
    if (ClientSocketTCP == INVALID_SOCKET) {
        printf("accept TCP failed with error: %d\n", WSAGetLastError());
        cleanup(1);
    }

    // No longer need server socket
    closesocket(ListenSocketTCP);

    // Accept a client socket
    ClientSocketUDP = accept(ClientSocketUDP, NULL, NULL);
    if (ClientSocketUDP == INVALID_SOCKET) {
        printf("accept UDP failed with error: %d\n", WSAGetLastError());
        cleanup(1);
    }

    // No longer need server socket
    
    closesocket(ListenSocketUDP);

    // Receive until the peer shuts down the connection
    do {

        iResult = recv(ClientSocketTCP, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            printf("Bytes TCP received: %d\n", iResult);

            // Echo the buffer back to the sender
            iSendResult = send(ClientSocketTCP, recvbuf, iResult, 0);
            if (iSendResult == SOCKET_ERROR) {
                printf("send TCP failed with error: %d\n", WSAGetLastError());
                cleanup(1);
            }
            printf("Bytes TCP sent: %d\n", iSendResult);
        }
        else if (iResult == 0)
            printf("Connection TCP closing...\n");
        else {
            printf("recv TCP failed with error: %d\n", WSAGetLastError());
            cleanup(1);
        }

        iResult = recv(ClientSocketUDP, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            printf("Bytes UDP received: %d\n", iResult);

            // Echo the buffer back to the sender
            iSendResult = send(ClientSocketUDP, recvbuf, iResult, 0);
            if (iSendResult == SOCKET_ERROR) {
                printf("send UDP failed with error: %d\n", WSAGetLastError());
                cleanup(1);
            }
            printf("Bytes UDP sent: %d\n", iSendResult);
        }
        else if (iResult == 0)
            printf("Connection UDP closing...\n");
        else {
            printf("recv UDP failed with error: %d\n", WSAGetLastError());
            cleanup(1);
        }

    } while (iResult > 0);

    // cleanup
    cleanup(0);
}

void cleanup() {
    cleanup(0);
}

int cleanup(int code) {
    // shutdown the connection since no more data will be sent
    int iResult;
    printf("Cleaning up...");
    if (ListenSocketTCP != INVALID_SOCKET) {
        closesocket(ListenSocketTCP);
    }
    if (ClientSocketTCP != INVALID_SOCKET) {
        iResult = shutdown(ClientSocketTCP, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            printf("shutdown TCP failed with error: %d\n", WSAGetLastError());
            code = 1;
        }
        closesocket(ClientSocketTCP);
    }

    if (ListenSocketUDP != INVALID_SOCKET) {
        closesocket(ListenSocketUDP);
    }
    if (ClientSocketUDP != INVALID_SOCKET) {
        iResult = shutdown(ClientSocketUDP, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            printf("shutdown UDP failed with error: %d\n", WSAGetLastError());
            code = 1;
        }
        closesocket(ClientSocketUDP);
    }

    if (resultTCP != NULL)
        freeaddrinfo(resultTCP);

    if (resultUDP != NULL)
        freeaddrinfo(resultUDP);
    WSACleanup();

    exit(code);
}