/**
*
* THIS CODE HAS ORIGINATED AND BEEN MODIFIED FROM https://docs.microsoft.com/en-us/windows/win32/winsock/complete-client-code AND OTHER SOURCES
*
**/



// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

// Need to link with Ws2_32.lib

// #pragma comment (lib, "Mswsock.lib")
#include "../common.h"

int cleanup(int r);
void cleanup();

string ADDR = "127.0.0.1";

SOCKET TCP = INVALID_SOCKET;
struct addrinfo* resultTCP = NULL;
struct filehandler h;
bool handle = false, connectionActive, lis = false;
HANDLE listenThread;

string cmdstr;

unsigned __stdcall listen(void* n) {

	sockaddr_in clientInfo;
	int size = sizeof(clientInfo);

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
		if (lis) {
			int iResult_ = recv(TCP, h.buf, h.buflen, 0);
			if (iResult_ > 0) {
				if (!handle) {
					handle = true;
					char* cmd;
					getCmd(&cmd, h.buf);
					cmdstr = string(cmd);

					string cmdstr;
					//send/recv file- "[number] [data]" as a packet
					switch (h.status) {
					case 0:
						printf("Server sent %s.", cmd);
						lis = false;
						break;
					case 1:
						switch (h.sendrecv) {
						case 1: //received packet: size of data received (client sending)
							handleFileWrite(&h, &TCP, "server", ADDR.c_str(), iResult_);
							break;
						case 0: //server sending
							handleFileRead(&h, &TCP, "server", ADDR.c_str());
							break;
						}
					}
					if (h.status == 0) lis = false;

					//printf("Bytes TCP sent: %d; message: \"%s\"; client: %s\n", iResult_, recvbuf, addr);
					//do {
						//if (listenUDP) {
					if (cmd != NULL) free(cmd);
				}

				handle = false;
			}
			else if (iResult_ == 0) {
				printf("Connection TCP closing...\n");
				cleanupFileHandler(&h);

				cleanup();

			}

			else {
				printf("recv TCP failed with error: %d (server probably crashed)\n", WSAGetLastError());
				cleanupFileHandler(&h);
				cleanup(1);

			}
			if (h.iSendResult == SOCKET_ERROR) {
				printf("send TCP failed with error: %d\n", WSAGetLastError());
				h.iSendResult = send(TCP, "-1", strlen("-1") + 1, 0); //general error
				cleanupFileHandler(&h);
			}
		}
		else {
			Sleep(1);
		}
	} while (h.keepLooping);

	cleanup();
	return NULL;

}

int __cdecl main(int argc, char** argv)
{
	setup();

	if (argc > 1) {
		ADDR = argv[1];
		if (argc > 2) {
			TCPORT = argv[2];
		}
	}


	struct addrinfo* ptr = NULL,
		hintsTCP;

	/*// Validate the parameters
	if (argc != 2) {
		printf("usage: %s server-name\n", argv[0]);
		return 1;
	}*/

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

	listenThread = (HANDLE)_beginthreadex(NULL, 0, &listen, (void*)NULL, 0, NULL);
	resetFileHandler(&h);

	while (1) {
		if (h.status == 0) {
			printf("\n> ");
			string in;
			getline(cin, in);

			char* c = (char*)malloc(in.length() * sizeof(char) + 1);
			memcpy(c, in.c_str(), in.length() + 1);
			char* cmd;
			getCmd(&cmd, c);
			cmdstr = string(cmd);
			h.path = in.substr(strlen(cmd) + 1);
			if (strcmp(cmd, "exit") == 0) {
				printf("Closing connection and shutting down...");
				free(cmd);
				cleanup(0);
			}
			else if (strcmp(cmd, "iWant") == 0) {
				resetFileHandler(&h);
				initValues(&h, 1);
				h.f = fopen(h.path.c_str(), "wb");
				if (!h.f) {
					printf("File %s could not be made!\n", h.path.c_str());
					resetFileHandler(&h);
				}
				else {
					h.buflen = DEFAULT_BUFLEN + 1 + getIndexSize(h.index);
					h.buf = (char*)realloc(h.buf, h.buflen); //account for ulong, a space, fbuf
					if (!h.buf) {
						printf("Unable to allocate receiving buffer memory, aborting.");
						cleanupFileHandler(&h);
						cleanup(1);
					}
					else {
						h.tempbuflen = DEFAULT_BUFLEN;
						if (h.tempbuf) h.tempbuf = (char*)realloc(h.tempbuf, h.tempbuflen);
						else h.tempbuf = (char*)calloc(h.tempbuflen, 1);
						if (!h.tempbuf) {
							printf("Unable to allocate receiving buffer memory, aborting.");
							cleanupFileHandler(&h);
						}
						else {
							sprintf(h.buf, "iWant %s\0", h.path.c_str());
							h.iSendResult = send(TCP, h.buf, strlen(h.buf) + 1, 0);
							printf("Requested file: %s\n", h.buf);
							h.index++;
							lis = true;
						}
					}
				}
			}
			else if (strcmp(cmd, "uTake") == 0) {
				resetFileHandler(&h);
				initValues(&h, 0);
				h.f = fopen(h.path.c_str(), "rb");
				if (!h.f) {
					printf("What you talkin bout Willis?  I aint seen that file nowhere! %s\n", h.path.c_str());
					resetFileHandler(&h);
				}
				else {
					rewind(h.f);
					fseek(h.f, 0L, SEEK_END);
					h.fRemaining = ftell(h.f);
					rewind(h.f);
					sprintf(h.buf, "uTake %s\0", h.path.c_str());
					h.iSendResult = send(TCP, h.buf, strlen(h.buf) + 1, 0);
					printf("Starting file sending with server\n");
					printf("Init sending file: %s\n", h.buf);
					lis = true;
				}

			}
			else {
				printf("That just ain't right! iWant or uTake [file]");
			}
			free(c);
			free(cmd);
		}
		else {
			Sleep(1);
		}
	}




	// cleanup
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
		cleanupFileHandler(&h);

		exit(code);
	}
}