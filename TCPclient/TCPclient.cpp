/**
* 
* THIS CODE HAS ORIGINATED AND BEEN MODIFIED FROM https://docs.microsoft.com/en-us/windows/win32/winsock/complete-client-code AND OTHER SOURCES
* 
**/

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <string>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512


using namespace std;

int cleanup(int r);
void cleanup();

string TCPORT = "27015";
string ADDR = "a.quantonium.net";

WSADATA wsaData;
SOCKET TCP = INVALID_SOCKET;
struct addrinfo* resultTCP = NULL;
chrono::high_resolution_clock::time_point startTCP;
char* sendbuf = (char*)malloc(4096);
char recvbuf[DEFAULT_BUFLEN];
int iResult;
int recvbuflen = DEFAULT_BUFLEN;
ofstream myfile("TCP.csv", ios::out | ios::app);
bool connectionActive = false;

int __cdecl main(int argc, char** argv)
{
    atexit(cleanup);
    

    if (!myfile.is_open()) {
        printf("Unable to open file");
        return 1;
    }

    struct addrinfo* ptr = NULL,
        hintsTCP;
    

    chrono::high_resolution_clock::time_point end;

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

    // Resolve the server address and port
    iResult = getaddrinfo(ADDR.c_str(), TCPORT.c_str(), &hintsTCP, &resultTCP);
    if (iResult != 0) {
        printf("getaddrinfo TCP failed with error: %d\n", iResult);
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

    if (TCP == INVALID_SOCKET) {
        printf("Unable to connect to server! TCP\n");
        cleanup(1);
    }

    printf("Server connection established TCP\n");
    connectionActive = true;
    while (1) {
            printf("Send a message: ");
            cin >> sendbuf;
            printf("Sending via TCP \"%s\"...\n", sendbuf);
            startTCP = chrono::high_resolution_clock::now();
            iResult = send(TCP, sendbuf, strlen(sendbuf)+1, 0);
            if (iResult == SOCKET_ERROR) {
                printf("send failed with error: %d\n", WSAGetLastError());
                cleanup(1);
            }
            printf("Bytes Sent TCP: %ld\n", iResult);
            printf("Listening TCP...\n");
            iResult = recv(TCP, recvbuf, recvbuflen, 0);
            if (iResult > 0) {
                end = chrono::high_resolution_clock::now();
                printf("Bytes received TCP: %d\n", iResult);
                printf("Received \"%s\". Time: %d\n", recvbuf, (end - startTCP));
                long r = (end - startTCP).count();
                myfile << "" << iResult << "," << r << "\n";
            }
            else if (iResult == 0) {
                printf("Connection UDP closing...\n");
                cleanup();
            }

            else {
                printf("recv UDP failed with error: %d\n", WSAGetLastError());
                cleanup(1);
            }
        }


 

    // cleanup
    cleanup(0);
}

void cleanup() {
    cleanup(0);
}

int cleanup(int code) {
    // shutdown the connection since no more data will be sent
    printf("Cleaning up...");
    if (TCP != INVALID_SOCKET) {
        if (connectionActive) {
            iResult = shutdown(TCP, SD_SEND);
            if (iResult == SOCKET_ERROR) {
                printf("shutdown TCP failed with error: %d\n", WSAGetLastError());
                code = 1;
            }
        }
        closesocket(TCP);
    }

    if (resultTCP != NULL)
        freeaddrinfo(resultTCP);

    WSACleanup();

    myfile.close();

    free(sendbuf);

    exit(code);
}