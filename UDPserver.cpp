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
#include <signal.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

using namespace std;

#define DEFAULT_BUFLEN 512
string UDPORT = "27016";


SOCKET ListenSocketUDP = INVALID_SOCKET;

struct sockaddr_in si_other;
int slen = sizeof(si_other);

struct addrinfo *resultUDP = NULL;
struct addrinfo hintsUDP;

int cleanup(int r);
void cleanup();
void _cleanup(int r) {
    printf("Signal received: %d. Terminating program...\n", r);
    cleanup(r);
}

WSADATA wsaData;
int iResult;

int iSendResult;
char recvbuf[DEFAULT_BUFLEN];
int recvbuflen = DEFAULT_BUFLEN;



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

    ZeroMemory(&hintsUDP, sizeof(hintsUDP));
    hintsUDP.ai_family = AF_INET;
    hintsUDP.ai_socktype = SOCK_DGRAM;
    hintsUDP.ai_protocol = IPPROTO_UDP;
    hintsUDP.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, UDPORT.c_str(), &hintsUDP, &resultUDP);
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

    //UDP doesn't have listen/accept, so simply take any data just received and send back to sender. No need to keep track of who's still connected.
    //Alright I could do keep-alive but I'll just let the client fail on server shutdown instead

    // Receive until the peer shuts down the connection
    char addr[INET_ADDRSTRLEN];
    do {
        iResult = recvfrom(ListenSocketUDP, recvbuf, recvbuflen, 0, (SOCKADDR *)&si_other, &slen);
        inet_ntop(AF_INET, &si_other.sin_addr, addr, sizeof(addr));
        if (iResult > 0) {
            printf("Bytes UDP received: %d; message: \"%s\"; client: %s\n", iResult, recvbuf, addr);

            // Echo the buffer back to the sender
            iSendResult = sendto(ListenSocketUDP, recvbuf, iResult, 0, (SOCKADDR *)&si_other, slen);
            if (iSendResult == SOCKET_ERROR) {
                printf("send UDP failed with error: %d\n", WSAGetLastError());
                cleanup(1);
            }
            printf("Bytes UDP sent: %d; message: \"%s\"; client: %s\n", iResult, recvbuf, addr);
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
        printf("Cleaning up... (Errors my occur as sockets DC)\n");


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
    
}