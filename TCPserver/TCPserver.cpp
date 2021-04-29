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

HANDLE listenThread;



int cleanup(int r);
void cleanup();

bool keepActive = true;

sockaddr_in clientInfo;
int Csize = sizeof(clientInfo);
char addr[INET_ADDRSTRLEN];

unsigned __stdcall ClientSession(void* data) {
	socks.push_back((SOCKET)data);
	SOCKET* ClientSocket = &socks.back();
	long recvbuflen = DEFAULT_BUFLEN;
	long sendbuflen = DEFAULT_BUFLEN;
	long fbuflen = DEFAULT_BUFLEN;
	char* sendbuf = (char*)calloc(sendbuflen, 1);
	char* fbuf = (char*)calloc(fbuflen, 1);
	char* recvbuf = (char*)calloc(recvbuflen, 1);
	long index = 1; //biiiiiiiiiiiiig files
	int iSendResult = 0;
	int status = 0;
	int sendrecv = 0; //0 for send, 1 for recv
	bool foundMax = false;
	bool dataDropped = false;
	bool finishNextLoop = false , canEnd;
	bool keepLooping = true;
	FILE* f = NULL;
	sockaddr_in clientInfo;
	int size = sizeof(clientInfo);
	getpeername(*ClientSocket, (struct sockaddr*)&clientInfo, &size);
	char addr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &clientInfo.sin_addr, addr, sizeof(addr));
	printf("Client connected: %s\n", addr);
	string path;
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
		int iResult_ = recv(*ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult_ > 0) {
			string cmdstr;
			char* cmd;
			getCmd(&cmd, recvbuf);
			cmdstr = string(recvbuf);
			long s = strtol(recvbuf, NULL, 10);
			switch (status) {
			case 0: //command- "iWant/uTake [filepath/filename]" as a string
				recvbuf[iResult_] = '\0';
				printf("%s\n", recvbuf);
				path = cmdstr.substr(strlen(cmd) + 1, iResult_ - (strlen(cmd) + 1));
				if (strcmp(cmd, "iWant") == 0) {
					f = fopen(path.c_str(), "rb");
					if (!f) {
						printf("What you talkin bout Willis?  I aint seen that file nowhere! %s\n", path.c_str());
						printf("Cannot open file %s\n", path.c_str());
						iSendResult = send(*ClientSocket, "-2", strlen("-2") + 1, 0); //no file
					}
					else {
						index = 1;
						status = 1;
						sendrecv = 0;
						foundMax = false;
						dataDropped = false;
						finishNextLoop = false;
						fbuflen = DEFAULT_BUFLEN;
						recvbuflen = DEFAULT_BUFLEN;
						sendbuflen = DEFAULT_BUFLEN;
						fbuf = (char*)realloc(fbuf, fbuflen);
						rewind(f);
						int n = fread(fbuf, 1, fbuflen, f);
						fflush(f);
						bool end = false;
						if (n < fbuflen) {

							fbuflen = n;
							end = true;
						}
						if (fbuflen != 0) {
							sendbuflen = fbuflen + 1 + getIndexSize(index);
							sendbuf = (char*)realloc(sendbuf, sendbuflen); //account for ulong, a space, fbuf
							sprintf(sendbuf, "%ld ", index);
							for (int j = 0; j < fbuflen; j++) {
								*(sendbuf + j + 1 + getIndexSize(index)) = *(fbuf + j);
							}
							iSendResult = send(*ClientSocket, sendbuf, sendbuflen, 0);
							printf("Initial bytes TCP sent: %d; index: %ld; client: %s\n", sendbuflen, index, addr);
						}
						else {
							printf("Done writing to client %s.\n", addr);
							iSendResult = send(*ClientSocket, "Done ", sizeof("Done ") + 1, 0);
							fclose(f);
							recvbuflen = DEFAULT_BUFLEN;
							recvbuf = (char*)realloc(recvbuf, recvbuflen);
							status = 0;
						}
					}
				}
				else if (strcmp(cmd, "uTake") == 0) {
					f = fopen(path.c_str(), "wb");
					if (!f) {
						printf("Cannot create file %s\n", path.c_str());
						iSendResult = send(*ClientSocket, "-2", strlen("-2") + 1, 0); //file creation fail
					}
					else {
						index = 1;
						status = 1;
						sendrecv = 1;
						foundMax = false;
						dataDropped = false;
						finishNextLoop = false;
						fbuflen = DEFAULT_BUFLEN;
						sendbuflen = DEFAULT_BUFLEN;
						fbuf = (char*)realloc(fbuf, fbuflen);
						recvbuflen = fbuflen + 1 + getIndexSize(index);
						recvbuf = (char*)realloc(recvbuf, recvbuflen); //account for ulong, a space, fbuf
						iSendResult = send(*ClientSocket, "ready ", strlen("ready ") + 1, 0);
						printf("Starting file retrieval with client: %s\n", addr);
					}

				}
				break;
			case 1: //send/recv file- "[number] [data]" as a packet
				switch (sendrecv) {
				case 0: //received packet: size of data received (client is receiving)

					if (s < 0) { //something went wrong
						printf("Client %s error code %ld\n", addr, s);
						fclose(f);
						remove(path.c_str());
						status = 0;
						recvbuflen = DEFAULT_BUFLEN;
						recvbuf = (char*)realloc(recvbuf, recvbuflen);
						status = 0;
					}
					else {
						canEnd = true;
						printf("Client replied with size %ld.\n", s);
						if (s < fbuflen) { //data lost- reduce buffer size
							index--;
							fseek(f, -1 * fbuflen, SEEK_CUR);
							fbuflen /= 2;
							dataDropped = true;
							canEnd = false;

						}
						else if (dataDropped) { //data was previously lost, but not this time- max buffer size found
							foundMax = true;
						}
						else if (!foundMax) { //no data lost yet, attempt to increase buffer size
							fbuflen *= 2;
						}
						if (fbuflen == 0) { //too much data has been lost, terminate transfer
							iSendResult = send(*ClientSocket, "-4", strlen("-4") + 1, 0);
							fclose(f);
							recvbuflen = DEFAULT_BUFLEN;
							recvbuf = (char*)realloc(recvbuf, recvbuflen);
							status = 0;
						}
						else if (!finishNextLoop) {
							fbuf = (char*)realloc(fbuf, fbuflen);
							int n = fread(fbuf, 1, fbuflen, f);
							fflush(f);
							bool end = false;
							if (n < fbuflen) {

								fbuflen = n;
								end = true;
							}
							if (fbuflen != 0) {
								sendbuflen = fbuflen + 1 + getIndexSize(index);
								sendbuf = (char*)realloc(sendbuf, sendbuflen);
								sprintf(sendbuf, "%ld ", index);
								for (int j = 0; j < fbuflen; j++) {
									*(sendbuf + j + 1 + getIndexSize(index)) = *(fbuf + j);
								}
								iSendResult = send(*ClientSocket, sendbuf, sendbuflen, 0);
								printf("Bytes TCP sent: %d; index: %ld; client: %s\n", sendbuflen, index, addr);
							}
							else {
								printf("Done writing to client %s.\n", addr);
								iSendResult = send(*ClientSocket, "Done ", sizeof("Done ") + 1, 0);
								fclose(f);
								recvbuflen = DEFAULT_BUFLEN;
								recvbuf = (char*)realloc(recvbuf, recvbuflen);
								status = 0;
							}
							if (canEnd && end) {
								finishNextLoop = true;
							}
						}
						else {
							printf("Done writing to client %s.\n", addr);
							iSendResult = send(*ClientSocket, "Done ", sizeof("Done ") + 1, 0);
							fclose(f);
							recvbuflen = DEFAULT_BUFLEN;
							recvbuf = (char*)realloc(recvbuf, recvbuflen);
							status = 0;
						}
					}
					break;
				case 1: //client sending
					if (strcmp(cmd, "Done") == 0) { //All doen!!!!1!11!1
						printf("Done receiving from client %s.\n", addr);
						fwrite(fbuf, 1, fbuflen, f);
						fflush(f);
						fclose(f);
						recvbuflen = DEFAULT_BUFLEN;
						recvbuf = (char*)realloc(recvbuf, recvbuflen);
						status = 0;
					}
					else {
						long ind = strtol(cmd, NULL, 10);
						if (ind < 0) { //something went wrong
							printf("Client %s error code %ld\n", addr, ind);
							fclose(f);
							remove(path.c_str());
							status = 0;
							recvbuflen = DEFAULT_BUFLEN;
							recvbuf = (char*)realloc(recvbuf, recvbuflen);
							status = 0;
						}
						else if (ind == 0) { //index received was 0. Just skipping these, for some reason extra indexes get sent or something so yeah
							printf("Index received was 0, skipping rogue packet\n");
						}
						else {
							printf("Received index from client %s: %ld; size: %d;\n", addr, ind, iResult_);
							long l = iResult_ - (strlen(cmd) + 1);
							if (ind+1 == index) { //next set of data
								if (ind > 1) {
									fwrite(fbuf, 1, fbuflen, f);
									fflush(f);
								}
								if (dataDropped) {
									foundMax = true;
								}
								else if (!foundMax) {
									recvbuflen = (l*2) + 1 + getIndexSize(index);
									recvbuf = (char*)realloc(recvbuf, recvbuflen);
								}
							}
							else { //data loss, or something else mysterious
								dataDropped = true;
								index--;
								recvbuflen = ((recvbuflen - (1 + getIndexSize(index))) / 2) + 1 + getIndexSize(index);
								recvbuf = (char*)realloc(recvbuf, recvbuflen);
							}
							fbuflen = l;
							fbuf = (char*)realloc(fbuf, fbuflen);
							for (int j = 0; j < fbuflen; j++) {
								*(fbuf + j) = *(recvbuf + j + (strlen(cmd) + 1));
							}
							sprintf(sendbuf, "%ld", fbuflen); //send back the length of the data retrieved to be written to the file
							iSendResult = send(*ClientSocket, sendbuf, strlen(sendbuf)+1, 0);
							printf("Size sent: %s; client: %s\n", sendbuf, addr);
						}
					}
					break;
				}
				break;
			}
			index++;
			free(cmd);


			// Echo the buffer back to the sender
			//for (auto& c : recvbuf) c = toupper(c);
			//iSendResult = send(*ClientSocket, recvbuf, iResult_, 0);
			if (iSendResult == SOCKET_ERROR) {
				printf("send TCP failed with error: %d\n", WSAGetLastError());
				iSendResult = send(*ClientSocket, "-1", strlen("-1") + 1, 0); //general error
				if (f) fclose(f);
			}
			//printf("Bytes TCP sent: %d; message: \"%s\"; client: %s\n", iResult_, recvbuf, addr);
			//do {
				//if (listenUDP) {

		}
		else if (iResult_ == 0) {
			printf("Connection TCP closing...\n");
			if (f) fclose(f);
			keepLooping = false;

		}

		else {
			printf("recv TCP failed with error: %d (Note: Client could've disconnected their end)\n", WSAGetLastError());
			if (f) fclose(f);
			keepLooping = false;

		}

	} while (keepLooping && keepActive);

	//close connection
	int iResult_ = shutdown(*ClientSocket, SD_SEND);
	if (iResult_ == SOCKET_ERROR) {
		printf("shutdown TCP failed with error: %d\n", WSAGetLastError());
	}
	printf("Connection with %s terminated successfully.", addr);
	free(fbuf);
	free(recvbuf);
	free(sendbuf);
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

	listenThread = (HANDLE)_beginthreadex(NULL, 0, &handleConnects, (void*)NULL, 0, NULL);

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