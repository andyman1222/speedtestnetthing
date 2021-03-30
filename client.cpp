#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <thread>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define UDPORT "27016"
#define TCPORT "27015"
#define ADDR "a.quantonium.net"

using namespace std;

int cleanup(int r);
void cleanup();

SOCKET TCP = INVALID_SOCKET, UDP = INVALID_SOCKET;
struct addrinfo *resultTCP = NULL, *resultUDP = NULL;
time_t startTCP, startUDP;
bool recvTCP = false, recvUDP = false;

void TCPrecv() {
    string sendbuf;
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    time_t end;
    
    do {
        //if (recvTCP) {
            iResult = recv(TCP, recvbuf, recvbuflen, 0);
            if (iResult > 0) {
                time(&end);
                printf("Bytes received TCP: %d\n", iResult);
                printf("Received \"%s\". Time: %d", recvbuf, (end - startTCP));
                recvTCP = false;
            }

            else if (iResult == 0)
                printf("Connection closed TCP\n");
            else
                printf("TCP recv failed with error: %d\n", WSAGetLastError());
        //}
        //else {
         //   Sleep(1);
        //}
        

    } while (iResult > 0);
    cleanup();
    
}

void UDPrecv() {
    string sendbuf;
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    time_t end;
    
    do {
        //if (recvTCP) {
            iResult = recv(UDP, recvbuf, recvbuflen, 0);
            if (iResult > 0) {
                time(&end);
                printf("Bytes received UDP: %d\n", iResult);
                printf("Received \"%s\". Time: %d", recvbuf, (end - startUDP));
                recvTCP = false;
            }

            else if (iResult == 0)
                printf("Connection closed UDP\n");
            else
                printf("UDP recv failed with error: %d\n", WSAGetLastError());
        //}
        //else {
        //    Sleep(1);
        //}
        

    } while (iResult > 0);
    cleanup();
    
}

int __cdecl main(int argc, char** argv)
{
    atexit(cleanup);
    WSADATA wsaData;
    
    
    struct addrinfo* ptr = NULL,
        hintsTCP, hintsUDP;
    string sendbuf;
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    time_t start, end;

    /*// Validate the parameters
    if (argc != 2) {
        printf("usage: %s server-name\n", argv[0]);
        return 1;
    }*/

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hintsTCP, sizeof(hintsTCP));
    hintsTCP.ai_family = AF_UNSPEC;
    hintsTCP.ai_socktype = SOCK_STREAM;
    hintsTCP.ai_protocol = IPPROTO_TCP;

    ZeroMemory(&hintsUDP, sizeof(hintsUDP));
    hintsUDP.ai_family = AF_UNSPEC;
    hintsUDP.ai_socktype = SOCK_DGRAM;
    hintsUDP.ai_protocol = IPPROTO_UDP;

    // Resolve the server address and port
    iResult = getaddrinfo(ADDR, TCPORT, &hintsTCP, &resultTCP);
    if (iResult != 0) {
        printf("getaddrinfo TCP failed with error: %d\n", iResult);
        cleanup(1);
    }

    // Resolve the server address and port
    iResult = getaddrinfo(ADDR, UDPORT, &hintsUDP, &resultUDP);
    if (iResult != 0) {
        printf("getaddrinfo UDP failed with error: %d\n", iResult);
        cleanup(1);
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = resultTCP; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        TCP = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (TCP == INVALID_SOCKET) {
            printf("socket TCP failed with error: %ld\n", WSAGetLastError());
            cleanup(1);
        }

        // Connect to server.
        iResult = connect(TCP, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(TCP);
            TCP = INVALID_SOCKET;
            continue;
        }
        break;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = resultUDP; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        UDP = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (UDP == INVALID_SOCKET) {
            printf("socket UDP failed with error: %ld\n", WSAGetLastError());
            cleanup(1);
        }

        // Connect to server.
        iResult = connect(UDP, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(UDP);
            UDP = INVALID_SOCKET;
            continue;
        }
        break;
    }

    if (TCP == INVALID_SOCKET) {
        printf("Unable to connect to server! TCP\n");
        cleanup(1);
    }

    if (UDP == INVALID_SOCKET) {
        printf("Unable to connect to server! UDP\n");
        cleanup(1);
    }

    printf("Server connection established TCP and UDP\n");

    thread TCPt(TCPrecv);
    thread UDPt(UDPrecv);

    while (1) {
        printf("Send a message: ");
        cin >> sendbuf;
        printf("Sending via TCP...\n");
        time(&startTCP);
        recvTCP = true;
        iResult = send(TCP, sendbuf.c_str(), (int)strlen(sendbuf.c_str()), 0);
        if (iResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            cleanup(1);
        }
        printf("Bytes Sent: %ld\n", iResult);
        printf("Sending via UDP...");
        time(&startUDP);
        recvUDP = true;
        iResult = send(UDP, sendbuf.c_str(), (int)strlen(sendbuf.c_str()), 0);
        if (iResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            cleanup(1);
        }
        printf("Bytes Sent: %ld\n", iResult);        
    }

    recvTCP = true;
    recvUDP = true;
    TCPt.join();
    UDPt.join();

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
    if (TCP != INVALID_SOCKET) {
        iResult = shutdown(TCP, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            printf("shutdown TCP failed with error: %d\n", WSAGetLastError());
            code = 1;
        }
        closesocket(TCP);
    }

    if (UDP != INVALID_SOCKET) {
        iResult = shutdown(UDP, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            printf("shutdown UDP failed with error: %d\n", WSAGetLastError());
            code = 1;
        }
        closesocket(UDP);
    }

    if(resultTCP != NULL)
        freeaddrinfo(resultTCP);

    if (resultUDP != NULL)
        freeaddrinfo(resultUDP);
    WSACleanup();

    exit(code);
}