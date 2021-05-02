/**
*
* THIS CODE HAS ORIGINATED AND BEEN MODIFIED FROM https://docs.microsoft.com/en-us/windows/win32/winsock/complete-server-code AND OTHER SOURCES
*
**/




// Need to link with Ws2_32.lib

// #pragma comment (lib, "Mswsock.lib")
#include "../common.h"

SOCKET ListenSocketTCP = INVALID_SOCKET;
SOCKET ClientSocketTCP = INVALID_SOCKET;

struct addrinfo* resultTCP = NULL;
struct addrinfo hintsTCP;

std::list<HANDLE> con;
std::list<SOCKET> socks;
std::list<filehandler> handlers;

HANDLE listenThread;

int cleanup(int r);
void cleanup();

bool keepActive = true;

sockaddr_in clientInfo;
int Csize = sizeof(clientInfo);
char addr[INET_ADDRSTRLEN];

unsigned __stdcall ClientSession(void* data) {
	socks.push_back((SOCKET)data);
	SOCKET ClientSocket = socks.back();
	struct filehandler h;
	sockaddr_in clientInfo;
	int size = sizeof(clientInfo);
	getpeername(ClientSocket, (struct sockaddr*)&clientInfo, &size);
	char addr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &clientInfo.sin_addr, addr, sizeof(addr));
	printf("Client connected: %s\n", addr);
	resetFileHandler(&h);
	
	do {
		//SENDING PROTOCOL: Connection established via TCP; client requests to send or receive file;
		//sender will send chunk number, followed by chunk data;
		//receiver will hold bytes and reply with size;
		//if size matches sender, sends next chunk (delineated by a space), increasing buffer size if not already at limit;
		//reveiver will also write last chunk data into file
		//otherwise, sender will resend previous chunk, at a smaller buffer size (no data is written)
		//upon sending a chunk of size 0, file has completed
		//if size is negative, error occurred

		//Technically this protocol is not necessary for TCP, and will work well for UDP, but this is how I implement it
		int iResult_ = recv(ClientSocket, h.buf, h.buflen, 0);
		if (iResult_ > 0) {
			string cmdstr;
			char* cmd;
			getCmd(&cmd, h.buf);
			cmdstr = string(h.buf);
			
			switch (h.status) {
			case 0: //command- "iWant/uTake [filepath/filename]" as a string
				
				h.buf[iResult_] = '\0';
				if(iResult_ > strlen(cmd)+1){
					h.path = cmdstr.substr(strlen(cmd) + 1, iResult_ - (strlen(cmd) + 1));
					if (strcmp(cmd, "iWant") == 0) {
						initFileRead(&h, &ClientSocket, "client", addr);
					}
					else if (strcmp(cmd, "uTake") == 0) {
						initFileWrite(&h, &ClientSocket, "client", addr);
					}
				}
				break;
			case 1: //send/recv file- "[number] [data]" as a packet
				switch (h.sendrecv) {
				case 0: //received packet: size of data received (client is receiving)
					handleFileRead(&h, &ClientSocket, "client", addr);
					break;
				case 1: //client sending
					handleFileWrite(&h, &ClientSocket, "client", addr, iResult_);
					break;
				}
				break;
			}
			
			free(cmd);
		}
		else if (iResult_ == 0) {
			printf("Connection TCP closing...\n");
			cleanupFileHandler(&h);
		}

		else {
			printf("recv TCP failed with error: %d (Note: Client could've disconnected their end)\n", WSAGetLastError());
			cleanupFileHandler(&h);
		}
		if (h.iSendResult == SOCKET_ERROR) {
			printf("send TCP failed with error: %d\n", WSAGetLastError());
			h.iSendResult = send(ClientSocket, "-1", strlen("-1") + 1, 0); //general error
			cleanupFileHandler(&h);
		}
	} while (h.keepLooping && keepActive);

	//close connection
	int iResult_ = shutdown(ClientSocket, SD_SEND);
	if (iResult_ == SOCKET_ERROR) {
		printf("shutdown TCP failed with error: %d\n", WSAGetLastError());
	}
	printf("Connection with %s terminated successfully.", addr);
	cleanupFileHandler(&h);
	ExitThread(0);
}

unsigned __stdcall handleConnects(void* v) {
	// Accept a client socket
	while (keepActive) {
		ClientSocketTCP = accept(ListenSocketTCP, (struct sockaddr*)&clientInfo, &Csize);
		inet_ntop(AF_INET, &clientInfo.sin_addr, addr, sizeof(addr));
		if (ClientSocketTCP == INVALID_SOCKET) {
			printf("accept TCP failed with error: %d\n", WSAGetLastError());
		}
		else {
			//printf("Client connected: %s\n", addr);
			con.push_back((HANDLE)_beginthreadex(NULL, 0, &ClientSession, (void*)ClientSocketTCP, 0, NULL));
		}
	}
	ExitThread(0);
}

int __cdecl main(int argc, char** argv)
{
	setup();

	if (argc > 1) {
		TCPORT = argv[1];
	}
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
	iResult = getaddrinfo(NULL, TCPORT.c_str(), &hintsTCP, &resultTCP);
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

	printf("Type messages on server to send to all clients.\n");


	getsockname(ListenSocketTCP, (struct sockaddr*)&clientInfo, &Csize);
	inet_ntop(AF_INET, &clientInfo.sin_addr, addr, sizeof(addr));
	printf("Listening for connections on %s:%s...\n", addr, TCPORT.c_str());

	listenThread = (HANDLE)_beginthreadex(NULL, 0, handleConnects, (void*)NULL, 0, NULL);

	while (keepActive) {
		printf("Enter 'exit' to exit: ");
		string in;

		getline(cin, in);
		char* sendbuf = (char*)malloc(strlen(in.c_str()) + 1);
		memcpy(sendbuf, in.c_str(), strlen(in.c_str()) + 1);
		if (strcmp(sendbuf, "exit") == 0) {
			printf("Closing connection and shutting down...");
			cleanup(0);
		}
		//printf("Sending via TCP \"%s\" to all clients...\n", sendbuf);
		/*
		for (SOCKET TCP : socks) {
			if (TCP != INVALID_SOCKET) {
				int sum = 0;
				do {
					iResult = send(TCP, *(&sendbuf + sum), (strlen(sendbuf) + 1) - sum, 0);
					if (iResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						if (WSAGetLastError() == 10058) { //shutdown
							printf("Client shutdown; removing dead socket.\n");
							socks.remove(TCP);
							break;
						}
						else cleanup(1);
					}
					else {
						sum += iResult;
						printf("Bytes Sent TCP: %ld\n", iResult);
					}
				} while (sum < strlen(sendbuf) + 1);
			}
			else socks.remove(TCP);
		}
		*/
		free(sendbuf);
	}

	// cleanup
	cleanup(0);
}



bool clean = false;
int cleanup(int code) {
	if (!clean) {
		clean = true;
		// shutdown the connection since no more data will be sent
		printf("Cleaning up... (Error messages may occur as sockets DC)\n");
		keepActive = false;

		if (listenThread != NULL) WaitForSingleObject(listenThread, 1000);

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