#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define UDPORT "27016"


SOCKET ListenSocketUDP = INVALID_SOCKET;

struct sockaddr_in si_other;
int slen;

struct addrinfo *resultUDP = NULL;
struct addrinfo hintsUDP;

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

    ZeroMemory(&hintsUDP, sizeof(hintsUDP));
    hintsUDP.ai_family = AF_INET;
    hintsUDP.ai_socktype = SOCK_DGRAM;
    hintsUDP.ai_protocol = IPPROTO_UDP;
    hintsUDP.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, UDPORT, &hintsUDP, &resultUDP);
    if (iResult != 0) {
        printf("getaddrinfo UDP failed with error: %d\n", iResult);
        cleanup(1);
    }

    // Create a SOCKET for connecting to server
    ListenSocketUDP = socket(resultUDP->ai_family, resultUDP->ai_socktype, resultUDP->ai_protocol);
    if (ListenSocketUDP == INVALID_SOCKET) {
        printf("socket UDP failed with error: %ld\n", WSAGetLastError());
        cleanup(1);
    }

    // Setup the UDP listening socket
    iResult = bind(ListenSocketUDP, resultUDP->ai_addr, (int)resultUDP->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind UDP failed with error: %d\n", WSAGetLastError());
        cleanup(1);
    }

    freeaddrinfo(resultUDP);

    printf("Listening for connections...\n");
    

    // Receive until the peer shuts down the connection

    do {
        iResult = recvfrom(ListenSocketUDP, recvbuf, recvbuflen, 0, (struct sockaddr*)&si_other, &slen);
        if (iResult > 0) {
            printf("Bytes UDP received: %d; message: \"%s\"\n", iResult, recvbuf);

            // Echo the buffer back to the sender
            iSendResult = sendto(ListenSocketUDP, recvbuf, iResult, 0, (struct sockaddr*)&si_other, slen);
            if (iSendResult == SOCKET_ERROR) {
                printf("send UDP failed with error: %d\n", WSAGetLastError());
                cleanup(1);
            }
            printf("Bytes UDP sent: %d; message: \"%s\"\n", iSendResult, recvbuf);
        }
        else if (iResult == 0) {
            printf("Connection UDP closing...\n");
            cleanup();
        }

        else {
            printf("recv UDP failed with error: %d\n", WSAGetLastError());
            cleanup(1);
        }     
    } while (iResult > 0);
    cleanup();

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


    if (ListenSocketUDP != INVALID_SOCKET) {
        iResult = shutdown(ListenSocketUDP, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            printf("shutdown UDP failed with error: %d\n", WSAGetLastError());
            code = 1;
        }
        closesocket(ListenSocketUDP);
    }

    if (resultUDP != NULL)
        freeaddrinfo(resultUDP);
    WSACleanup();

    exit(code);
}