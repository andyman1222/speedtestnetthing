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
#include <signal.h>


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

string TCPORT = "27015";
string ADDR = "a.quantonium.net";

WSADATA wsaData;
SOCKET TCP = INVALID_SOCKET;
struct addrinfo* resultTCP = NULL;
chrono::high_resolution_clock::time_point startTCP;
char* sendbuf;
char* recvbuf;
int iResult;
int recvbuflen;
ofstream myfile("TCP.csv", ios::out | ios::app);
bool connectionActive = false;


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
			TCPORT = argv[2];
		}
	}

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

	printf("Attempting to connect %s:%s\n", ADDR.c_str(), TCPORT.c_str());

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
		string in;
		getline(cin, in);
		sendbuf = (char*)malloc(strlen(in.c_str()) + 1);
		memcpy(sendbuf, in.c_str(), strlen(in.c_str()) + 1);
		printf("Sending via TCP \"%s\"...\n", sendbuf);
		int sum = 0;
		startTCP = chrono::high_resolution_clock::now();
		do {
			iResult = send(TCP, *(&sendbuf + sum), (strlen(sendbuf) + 1) - sum, 0);
			if (iResult == SOCKET_ERROR) {
				printf("send failed with error: %d\n", WSAGetLastError());
				cleanup(1);
			}
			sum += iResult;
			printf("Bytes Sent TCP: %ld\n", iResult);
		} while (sum < strlen(sendbuf) + 1);
		printf("Listening TCP...\n");
		recvbuflen = (strlen(sendbuf) + 1);
		recvbuf = (char*)malloc(recvbuflen * sizeof(char));
		sum = 0;
		do {
			iResult = recv(TCP, *(&recvbuf + sum), recvbuflen - sum, 0);
			if (iResult > 0) {
				printf("Bytes received TCP: %d\n", iResult);
				sum += iResult;
			}
			else if (iResult == 0) {
				printf("Connection TCP closing...\n");
				cleanup();
			}

			else {
				printf("recv TCP failed with error: %d\n", WSAGetLastError());
				cleanup(1);
			}
		} while (sum < strlen(sendbuf) + 1);
		end = chrono::high_resolution_clock::now();
		long r = (end - startTCP).count();
		myfile << "" << iResult << "," << r << "," << sendbuf << "," << recvbuf << "\n";
		printf("Received \"%s\". Time: %d\n", recvbuf, r);
		free(recvbuf);
		free(sendbuf);
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

		if (sendbuf != NULL) free(sendbuf);

		if (recvbuf != NULL) free(recvbuf);

		exit(code);
	}
}