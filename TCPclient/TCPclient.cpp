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
void _cleanup(int r) {
	printf("Signal received: %d. Terminating program...\n", r);
	cleanup(r);
}

string ADDR = "127.0.0.1";


SOCKET TCP = INVALID_SOCKET;
struct addrinfo* resultTCP = NULL;
long recvbuflen = DEFAULT_BUFLEN;
long sendbuflen = DEFAULT_BUFLEN;
long fbuflen = DEFAULT_BUFLEN;
char* sendbuf = (char*)calloc(sendbuflen, 1);
char* fbuf = (char*)calloc(fbuflen, 1);
char* recvbuf = (char*)calloc(recvbuflen, 1);
long index = 0; //biiiiiiiiiiiiig files
int iResult_;
int iSendResult;
int status = 0;
int sendrecv = 0; //0 for send, 1 for recv
bool foundMax = false;
bool dataDropped = false;
bool connectionActive = false;
bool finishNextLoop = false, canEnd;
bool handle = false;
HANDLE listenThread;

string path;
string cmdstr;

FILE* f = NULL;

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
		iResult_ = recv(TCP, recvbuf, recvbuflen, 0);
		if (!handle) {
			handle = true;
			char* cmd;
			getCmd(&cmd, recvbuf);
			cmdstr = string(cmd);
			if (iResult_ > 0) {
				string cmdstr;
				//send/recv file- "[number] [data]" as a packet
				switch (sendrecv) {
				case 0: //received packet: size of data received (client sending)
					if (strcmp(cmd, "ready") == 0) { //ready 2 send data!!!1!1
						fbuflen = DEFAULT_BUFLEN;
						long oldbuflen = fbuflen/2;
						char* test = (char*)realloc(fbuf, fbuflen);
						if (test) fbuf = test;
						else {
							printf("Unable to allocate space for file buffer.");
							iSendResult = send(TCP, "-4", strlen("-4") + 1, 0);
							fclose(f);
							status = 0;
							cleanup(1);
						}
						int m = fread(fbuf, 1, fbuflen, f);
						fflush(f);
						bool end = false;
						canEnd = true;
						if (m < fbuflen) {
							fbuflen = m;
							end = true;
						}
						oldbuflen = sendbuflen;
						sendbuflen = fbuflen + 1 + getIndexSize(index);
						if (fbuflen != 0) {

							sendbuf = (char*)realloc(sendbuf, sendbuflen);
							sprintf(sendbuf, "%ld ", index);
							for (int j = 0; j < fbuflen; j++) {
								*(sendbuf + j + 1 + getIndexSize(index)) = *(fbuf + j);
							}
							iSendResult = send(TCP, sendbuf, sendbuflen, 0);
							printf("Initial bytes TCP sent: %d; index: %ld;\n", sendbuflen, index);
							if (canEnd && end) {
								finishNextLoop = true;
							}
						}
						else {
							printf("Done writing to server.\n");
							iSendResult = send(TCP, "Done ", sizeof("Done ") + 1, 0);
							fclose(f);
							recvbuflen = DEFAULT_BUFLEN;
							recvbuf = (char*)realloc(recvbuf, recvbuflen);
							status = 0;
						}
					}
					else {
						long size = strtol(recvbuf, NULL, 10);
						if (size < 0) { //something went wrong
							printf("Server error code %ld\n", size);
							if (size == -3) printf("File already exists on server!");
							else if (size == -2) printf("Error creating file on server!");
							else if (size == -4) printf("Server's buffer has gotten too small, aborting.");
							fclose(f);
							status = 0;
							recvbuflen = DEFAULT_BUFLEN;
							recvbuf = (char*)realloc(recvbuf, recvbuflen);
							status = 0;
						}
						else {
							canEnd = true;
							printf("Server replied with size %ld.\n", size);
							if (size < fbuflen) { //data lost- reduce buffer size
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
								iSendResult = send(TCP, "-4", strlen("-4") + 1, 0);
								fclose(f);
								recvbuflen = DEFAULT_BUFLEN;
								recvbuf = (char*)realloc(recvbuf, recvbuflen);
								status = 0;
							}
							else if (!finishNextLoop) {
								fbuf = (char*)realloc(fbuf, fbuflen);
								int m = fread(fbuf, 1, fbuflen, f);
								fflush(f);
								bool end = false;

								if (m < fbuflen) {
									fbuflen = m;
									end = true;
								}
								sendbuflen = fbuflen + 1 + getIndexSize(index);
								if (fbuflen != 0) {
									sendbuf = (char*)realloc(sendbuf, sendbuflen);
									sprintf(sendbuf, "%ld ", index);
									for (int j = 0; j < fbuflen; j++) {
										*(sendbuf + j + 1 + getIndexSize(index)) = *(fbuf + j);
									}
									iSendResult = send(TCP, sendbuf, sendbuflen, 0);
									printf("Bytes TCP sent: %d; index: %ld;\n", sendbuflen, index);
								}
								else {
									printf("Done writing to server.\n");
									iSendResult = send(TCP, "Done ", sizeof("Done ") + 1, 0);
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
								printf("Done writing to server.\n");
								iSendResult = send(TCP, "Done ", sizeof("Done ") + 1, 0);
								fclose(f);
								recvbuflen = DEFAULT_BUFLEN;
								recvbuf = (char*)realloc(recvbuf, recvbuflen);
								status = 0;
							}
						}
					}
					break;
				case 1: //server sending
					printf("%s\n", cmd);
					if (strcmp(cmd, "Done") == 0) { //All doen!!!!1!11!1
						printf("Done receiving from server.\n");
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
							printf("Server error code %ld\n", ind);
							if (ind == -3) printf("File already exists on server!");
							else if (ind == -2) printf("What you talkin bout Willis?  I aint seen that file nowhere!");
							else if (ind == -4) printf("Server's buffer has gotten too small, aborting.");
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
							printf("Data index received: %ld; size: %d;\n", ind, iResult_);
							long l = iResult_ - (strlen(cmd) + 1);
							if (ind + 1 == index) { //next set of data
								if (ind > 1) {
									fwrite(fbuf, 1, fbuflen, f);
									fflush(f);
								}
								if (dataDropped) {
									foundMax = true;
								}
								else if (!foundMax) {
									long oldrecvbuflen = recvbuflen;
									recvbuflen = (l * 2) + 1 + getIndexSize(index);
									char* test = (char*)realloc(recvbuf, recvbuflen);
									if (test) recvbuf = test;
									else {
										recvbuflen = oldrecvbuflen;
										foundMax = true;
									}
								}
							}
							else { //data loss, or something else mysterious
								dataDropped = true;
								index--;
								recvbuflen = ((recvbuflen -(1 + getIndexSize(index)))/2)+ 1 + getIndexSize(index);
								recvbuf = (char*)realloc(recvbuf, recvbuflen);
							}
							long oldfbuflen = fbuflen;
							fbuflen = iResult_ - (strlen(cmd) + 1);
							char* test = (char*)realloc(fbuf, fbuflen);
							if (test) recvbuf = test;
							else {
								fbuflen = oldfbuflen;
								foundMax = true;
							}
							for (int j = 0; j < fbuflen; j++) {
								*(fbuf + j) = *(recvbuf + j + (strlen(cmd) + 1));
							}
							sprintf(sendbuf, "%ld", fbuflen); //send back the length of the data retrieved to be written to the file
							iSendResult = send(TCP, sendbuf, strlen(sendbuf) + 1, 0);
							printf("Replied with size: %s;\n", sendbuf);
						}
					}
					break;
				}
				index++;



				// Echo the buffer back to the sender
				//for (auto& c : recvbuf) c = toupper(c);
				//iSendResult = send(*ClientSocket, recvbuf, iResult_, 0);
				if (iSendResult == SOCKET_ERROR) {
					printf("send TCP failed with error: %d\n", WSAGetLastError());
					iSendResult = send(TCP, "-1", strlen("-1") + 1, 0); //general error
					if (f) fclose(f);
					if (cmd != NULL) free(cmd);
				}
				//printf("Bytes TCP sent: %d; message: \"%s\"; client: %s\n", iResult_, recvbuf, addr);
				//do {
					//if (listenUDP) {

			}
			else if (iResult_ == 0) {
				printf("Connection TCP closing...\n");
				if (f) fclose(f);
				if (cmd != NULL) free(cmd);
				cleanup();

			}

			else {
				printf("recv TCP failed with error: %d (Note: Client could've disconnected)\n", WSAGetLastError());
				if (f) fclose(f);
				if (cmd != NULL) free(cmd);
				cleanup(1);

			}
			if (cmd != NULL) free(cmd);
			handle = false;
		}

	} while (iResult_ > 0 && connectionActive);

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

	while (1) {
		if (status == 0) {
			printf("\n> ");
			string in;
			getline(cin, in);

			char* c = (char*)malloc(in.length() * sizeof(char) + 1);
			memcpy(c, in.c_str(), in.length() + 1);
			char* cmd;
			getCmd(&cmd, c);
			printf("%s\n", cmd);
			cmdstr = string(cmd);

			if (strcmp(cmd, "exit") == 0) {
				printf("Closing connection and shutting down...");
				free(cmd);
				cleanup(0);
			}
			else if (strcmp(cmd, "iWant") == 0) {
				path = in.substr(strlen(cmd) + 1);
				printf("Requesting file %s...\n", path.c_str());

				f = fopen(path.c_str(), "wb");
				if (!f) {
					printf("File %s could not be made!\n", path.c_str());
				}
				else {
					index = 1;
					status = 1;
					sendrecv = 1;
					foundMax = false;
					dataDropped = false;
					finishNextLoop = false;
					fbuflen = DEFAULT_BUFLEN;
					recvbuflen = DEFAULT_BUFLEN;
					sendbuflen = DEFAULT_BUFLEN;
					sendbuf = (char*)realloc(sendbuf, sendbuflen); //account for ulong, a space, fbuf
					sprintf(sendbuf, "iWant %s\0", path.c_str());
					iSendResult = send(TCP, sendbuf, strlen(sendbuf) + 1, 0);
					printf("Requested file: %s\n", sendbuf);

				}
			}
			else if (strcmp(cmd, "uTake") == 0) {
				path = in.substr(strlen(cmd) + 1);
				printf("Sending file %s...\n", path.c_str());

				f = fopen(path.c_str(), "rb");
				if (!f) {
					printf("What you talkin bout Willis?  I aint seen that file nowhere! %s\n", path.c_str());
				}
				else {
					index = 1;
					status = 1;
					sendrecv = 0;
					foundMax = false;
					dataDropped = false;
					finishNextLoop = false;
					fbuflen = DEFAULT_BUFLEN;
					sendbuflen = DEFAULT_BUFLEN;
					recvbuflen = DEFAULT_BUFLEN;
					sendbuf = (char*)realloc(sendbuf, sendbuflen); //account for ulong, a space, fbuf
					sprintf(sendbuf, "uTake %s\0", path.c_str());
					iSendResult = send(TCP, sendbuf, strlen(sendbuf) + 1, 0);
					printf("Starting file sending with server\n");
					printf("Init sending file: %s\n", sendbuf);
					rewind(f);
				}

			}
			else if (strcmp(cmd, "test") == 0) { //TO TEST HETE SUTEWIOKURLKSEJ STUPID FILESYSTEM
				path = in.substr(strlen(cmd) + 1);
				printf("Opening and reading file %s...\n", path.c_str());
				f = fopen(path.c_str(), "rb");
				if (f) {
					char* k = (char*)malloc(sizeof(char));
					int i = 1;
					while (!feof(f)) {
						i++;
						fread(k, 1, i, f);
						fflush(f);
						for(int l = 0; l < i; l++)
							printf("%c", k[l]);
						k = (char*)realloc(k, i);

					}
					free(k);
					fclose(f);
				}
				else {
					printf("error: file not found\n");
				}
				printf("Done.");
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

		if (sendbuf != NULL) free(sendbuf);
		if (fbuf != NULL) free(fbuf);
		if (recvbuf != NULL) free(recvbuf);

		exit(code);
	}
}