/**
*
* THIS CODE HAS ORIGINATED AND BEEN MODIFIED FROM https://docs.microsoft.com/en-us/windows/win32/winsock/complete-client-code AND OTHER SOURCES
* 
* Fun fact, Rose's network does not support IPv6 (WAN).
*
**/

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <signal.h>
#include <string>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

using namespace std;

int cleanup(int r);
void cleanup();
void _cleanup(int r) {
    printf("Signal received: %d. Terminating program...\n", r);
    cleanup(r);
}

string UDPORT = "27016";
string ADDR = "a.quantonium.net";

WSADATA wsaData;
SOCKET UDP = INVALID_SOCKET;
struct addrinfo *resultUDP = NULL;
chrono::high_resolution_clock::time_point startUDP;
chrono::high_resolution_clock::time_point endUDP;
//char* sendbuf;
ofstream myfile("UDP.csv", ios::out | ios::app);
bool connectionActive = false;
char* recvbuf;
int iResult;
int recvbuflen;

bool test(char* sendbuf) {

    printf("Sending via UDP \"%s\"... Expected size: %d\n", sendbuf, strlen(sendbuf)+1);
    int sum = 0;

    startUDP = chrono::high_resolution_clock::now();
    do {
        iResult = send(UDP, *(&sendbuf + sum), (strlen(sendbuf) + 1) - sum, 0);
        if (iResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            free(recvbuf);
            return false;
        }
        sum += iResult;
        printf("Bytes Sent UDP: %ld\n", iResult);
    } while (sum < strlen(sendbuf) + 1);
    printf("Listening UDP...\n");
    recvbuflen = (strlen(sendbuf) + 1);
    recvbuf = (char*)malloc(recvbuflen * sizeof(char));
    sum = 0;
    do {
        iResult = recv(UDP, *(&recvbuf + sum), recvbuflen - sum, 0);
        if (iResult > 0) {

            printf("Bytes received UDP: %d\n", iResult);
            sum += iResult;
        }

        else if (iResult == 0) {
            printf("Connection closed UDP\n");
            free(recvbuf);
            return false;
        }

        else {
            printf("UDP recv failed with error: %d\n", WSAGetLastError());
            free(recvbuf);
            return false;
        }
            
    } while (sum < strlen(sendbuf) + 1);
    endUDP = chrono::high_resolution_clock::now();
    long r = (endUDP - startUDP).count();
    printf("Received \"%s\". Time: %d\n", recvbuf, r);
    myfile << "" << iResult << "," << r << "," << sendbuf << "," << recvbuf << "\n";
    free(recvbuf);
    return true;
}

int __cdecl main(int argc, char** argv)
{
    atexit(cleanup);
    signal(SIGINT, _cleanup);
    signal(SIGTERM, _cleanup);
    signal(SIGSEGV, _cleanup);
    signal(SIGABRT, _cleanup);
    signal(SIGFPE, _cleanup);
    
    if (argc > 1) {
        ADDR = argv[1];
        if (argc > 2) {
            UDPORT = argv[2];
        }
    }

    if (!myfile.is_open()) {
        printf("Unable to open file");
        return 1;
    }
    
    struct addrinfo* ptr = NULL,
        hintsUDP;

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

    ZeroMemory(&hintsUDP, sizeof(hintsUDP));
    hintsUDP.ai_family = AF_UNSPEC;
    hintsUDP.ai_socktype = SOCK_DGRAM;
    hintsUDP.ai_protocol = IPPROTO_UDP;

    // Resolve the server address and port
    iResult = getaddrinfo(ADDR.c_str(), UDPORT.c_str(), &hintsUDP, &resultUDP);
    if (iResult != 0) {
        printf("getaddrinfo UDP failed with error: %d\n", iResult);
        cleanup(1);
    }

    printf("Attempting to connect %s:%s\n", ADDR.c_str(), UDPORT.c_str());

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

    if (UDP == INVALID_SOCKET) {
        printf("Unable to connect to server! UDP\n");
        cleanup(1);
    }

    printf("Server connection established UDP\n");
    connectionActive = true;
    
    printf("Enter max bytes to send (2^x bytes will be sent): x=");
    int b;
    cin >> b;

    printf("Enter number of iterations per byte: ");
    int i;
    cin >> i;

    for (int ii = 0; ii < b; ii++) {
        char* s = (char*)malloc(pow(2, ii));
        for (int r = 0; r < i; r++) {
            for (int c = 0; c < pow(2, ii); c++)
                *(s+c) = (char)((rand() % 26) + 'a'); //fill up string with random chars from a to z
            *(s + ((int)pow(2, ii)-1)) = '\0';
            printf("Test %d: ", r+1);
            connectionActive = test(s);
            if (!connectionActive) {
                printf("Test failed on 2^%d, test %d.\n", ii, r);
                break;
                break;
                break;
            }
        }
        free(s);
    }

    // cleanup
    cleanup(0);
}

void cleanup() {
    cleanup(0);
}

bool clean = false;
int cleanup(int code) {
    if(!clean){
        clean = true;
        // shutdown the connection since no more data will be sent
        printf("Cleaning up... (errors may occur as sockets DC)\n");

        if (UDP != INVALID_SOCKET) {
            if (connectionActive) {
                iResult = shutdown(UDP, SD_SEND);
                if (iResult == SOCKET_ERROR) {
                    printf("shutdown UDP failed with error: %d\n", WSAGetLastError());
                    code = 1;
                }
            }
            closesocket(UDP);
        }

        if (resultUDP != NULL)
            freeaddrinfo(resultUDP);
        myfile.close();
        if (recvbuf != NULL) free(recvbuf);
        system("pause");
        WSACleanup();
        
        //if(sendbuf != NULL) free(sendbuf);
        
        exit(code);
    }
    
}