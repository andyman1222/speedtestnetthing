using namespace std;

string TCPORT = "27015";

struct filehandler {
	//UNLIMITED POWWWWWWAR (well at least to 16^16, not taking into account 2s c)
	long long recvbuflen = DEFAULT_BUFLEN;
	long long sendbuflen = DEFAULT_BUFLEN;
	long long fbuflen = DEFAULT_BUFLEN;
	char* sendbuf = (char*)calloc(sendbuflen, 1);
	char* fbuf = (char*)calloc(fbuflen, 1);
	char* recvbuf = (char*)calloc(recvbuflen, 1);
	long index = 1; //biiiiiiiiiiiiig files
	int iSendResult = 0;
	int status = 0;
	int sendrecv = 0; //0 for send, 1 for recv
	bool foundMax = false;
	bool dataDropped = false;
	bool finishNextLoop = false, canEnd;
	bool keepLooping = true;
	FILE* f = NULL;
	string path = "";
};

//strtok has been failing me so I made my own function
void getCmd(char** c, char* i) {
	int x = 0;
	while (*(i + x) != ' ' && *(i + x) != '\0') {
		x++;
	}
	*c = (char*)malloc(x + 1);
	memcpy(*c, i, x);
	*(*c + x) = '\0';
}

int getIndexSize(long i) {
	return (int)log10(i) + 1;
}

int cleanup(int);

void cleanup() {
	cleanup(0);
}

void _cleanup(int r) {
	printf("Signal received: %d. Terminating program...\n", r);
	cleanup(r);
}

void setup() {
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
		exit(1);
	}
}

void cleanupFileHandler(struct filehandler* h) {
	if (h != NULL) {
		if (h->f) fclose(h->f);
		if (h->keepLooping) {
			if (h->recvbuf) free(h->recvbuf);
			if (h->fbuf) free(h->fbuf);
			if (h->sendbuf) free(h->sendbuf);
			h->recvbuf = NULL;
			h->fbuf = NULL;
			h->sendbuf = NULL;
		}
		h->keepLooping = false;
	}
	free(h);
	h = NULL;
}

void resetFileHandler(struct filehandler* h) {
	if (h->f) fclose(h->f);
	h->index = 0;
	h->status = 0;
	h->foundMax = false;
	h->dataDropped = false;
	h->finishNextLoop = false;
	h->fbuflen = DEFAULT_BUFLEN;
	h->recvbuflen = DEFAULT_BUFLEN;
	h->sendbuflen = DEFAULT_BUFLEN;
	if (h->recvbuf) h->recvbuf = (char*)realloc(h->recvbuf, h->recvbuflen);
	else h->recvbuf = (char*)calloc(h->recvbuflen, 1);
	if (h->fbuf) h->fbuf = (char*)realloc(h->fbuf, h->fbuflen);
	else h->fbuf = (char*)calloc(h->fbuflen, 1);
	if (h->sendbuf) h->sendbuf = (char*)realloc(h->sendbuf, h->sendbuflen);
	else h->sendbuf = (char*)calloc(h->sendbuflen, 1);
	if (!h->recvbuf || !h->fbuf || !h->sendbuf) {
		printf("Unable to allocate memory for one or more buffers. aborting...");
		cleanupFileHandler(h);
	}
}

void initValues(struct filehandler* h, int sendrecvInit) {
	resetFileHandler(h);
	h->status = 1;
	h->index = 1;
	h->sendrecv = sendrecvInit;
}

long long testSize(int one, int two) {
	return one + two;
}

//Attempts to reallocate both the file buffer, and optionally either send or recv buffer, for more or less memory space
//bufExtSel: 0- no additional buffer; 1- resize sendbuf; 2- resize recvbuf;
//returns true if successful
bool requestNewSize(struct filehandler* h, long long proposedFbufSize, long long proposedBufOther, int bufOtherSel = 0, bool abortOnFail = false, SOCKET *sockptr = NULL) {
	char* test = NULL;
	long long* bufreflen = NULL;
	char** bufref = NULL;
	
	if (bufOtherSel == 1) {
		bufreflen = &h->sendbuflen;
		bufref = &h->sendbuf;
	}
	else if (bufOtherSel == 2) {
		bufreflen = &h->recvbuflen;
		bufref = &h->recvbuf;
	}
	h->fbuflen = proposedFbufSize;
	do {
		do {
			test = (char*)realloc(h->fbuf, h->fbuflen);
			if (!test) {
				h->foundMax = true;
				long long n = h->fbuflen / 2;
				long long diff = h->fbuflen - n;
				h->fbuflen = n;
				proposedBufOther -= diff;
				if (h->fbuflen == 0) {
					if (!sockptr) h->iSendResult = send(*sockptr, "-4", strlen("-4") + 1, 0); //no file
					if (abortOnFail) cleanupFileHandler(h);
					return false;
				}
			}
			else h->fbuf = test;
		} while (!test);
		
		if (bufOtherSel == 1 || bufOtherSel == 2) {
			test = NULL;
			*bufreflen = proposedBufOther;
			test = (char*)realloc(*bufref, *bufreflen);
			if (!test) {
				long long n = h->fbuflen / 2;
				long long diff = h->fbuflen - n;
				h->fbuflen = n;
				proposedBufOther -= diff;
				if (h->fbuflen <= 0 || *bufreflen <= 0) {
					if (!sockptr) h->iSendResult = send(*sockptr, "-4", strlen("-4") + 1, 0); //no file
					if (abortOnFail) cleanupFileHandler(h);
					return false;
				}
			}
			else *bufref = test;
		}
	} while (!test);
	return true;
}

void initFileRead(struct filehandler* h, SOCKET* sockptr, const char* recipient, const char* addr) {
	initValues(h, 0);
	h->f = fopen(h->path.c_str(), "rb");
	if (!h->f) {
		printf("What you talkin bout Willis?  I aint seen that file nowhere! %s\n", h->path.c_str());
		h->iSendResult = send(*sockptr, "-2", strlen("-2") + 1, 0); //no file
		resetFileHandler(h);
	}
	else {
		rewind(h->f);
		int n = fread(h->fbuf, 1, h->fbuflen, h->f);
		int tmp = fflush(h->f);
		if (tmp != 0) printf("fflush: %d", tmp);
		bool end = false;
		if (n < h->fbuflen) {
			h->fbuflen = n;
			end = true;
		}
		if (h->fbuflen != 0) {

			h->sendbuflen = h->fbuflen + 1 + getIndexSize(h->index);
			h->sendbuf = (char*)realloc(h->sendbuf, h->sendbuflen);
			if (!h->sendbuf) {
				printf("Unable to allocate sending buffer memory, aborting.");
				
				cleanupFileHandler(h);
			}
			else {
				sprintf(h->sendbuf, "%ld ", h->index);
				for (long long j = 0; j < h->fbuflen; j++) {
					*(h->sendbuf + j + 1 + getIndexSize(h->index)) = *(h->fbuf + j);
				}
				h->iSendResult = send(*sockptr, h->sendbuf, h->sendbuflen, 0);
				printf("Initial bytes TCP sent: %d; index: %ld; %s: %s\n", h->sendbuflen, h->index, recipient, addr);
			}
		}
		else {
			printf("Done writing to %s %s.\n", recipient, addr);
			h->iSendResult = send(*sockptr, "Done ", sizeof("Done ") + 1, 0);
			resetFileHandler(h);
		}
	}
}

void initFileWrite(struct filehandler* h, SOCKET* sockptr, const char* recipient, const char* addr) {
	initValues(h, 1);
	h->f = fopen(h->path.c_str(), "wb");
	if (!h->f) {
		printf("Cannot create file %s\n", h->path.c_str());
		h->iSendResult = send(*sockptr, "-2", strlen("-2") + 1, 0); //file creation fail
		resetFileHandler(h);
	}
	else {
		h->recvbuflen = h->fbuflen + 1 + getIndexSize(h->index);
		h->recvbuf = (char*)realloc(h->recvbuf, h->recvbuflen); //account for ulong, a space, fbuf
		if (!h->recvbuf) {
			printf("Unable to allocate receiving buffer memory, aborting.");
			cleanupFileHandler(h);
		}
		else {
			h->iSendResult = send(*sockptr, "ready ", strlen("ready ") + 1, 0);
			printf("Starting file retrieval with %s %s\n", recipient, addr);
		}
	}
}

void handleFileRead(struct filehandler* h, SOCKET* sockptr, const char* recipient, const char* addr) {
	long s = strtol(h->recvbuf, NULL, 10);
	char* cmd;
	getCmd(&cmd, h->recvbuf);
	if (strcmp(cmd, "ready") == 0) { //ready 2 send data!!!1!1 client only
		//fbuf already initialized from initValues

		long long m = fread(h->fbuf, 1, h->fbuflen, h->f);
		int tmp = fflush(h->f);
		if (tmp != 0) printf("fflush: %d", tmp);
		bool end = false;
		h->canEnd = true;
		if (m < h->fbuflen) {
			h->fbuflen = m;
			end = true;
		}
		h->sendbuflen = h->fbuflen + 1 + getIndexSize(h->index);
		if (h->fbuflen != 0) {
			sprintf(h->sendbuf, "%ld ", h->index);
			for (long long j = 0; j < h->fbuflen; j++) {
				*(h->sendbuf + j + 1 + getIndexSize(h->index)) = *(h->fbuf + j);
			}
			h->iSendResult = send(*sockptr, h->sendbuf, h->sendbuflen, 0);
			printf("Initial bytes TCP sent: %d; index: %ld;\n", h->sendbuflen, h->index);
			if (h->canEnd && end) {
				h->finishNextLoop = true;
			}
		}
		else {
			printf("Done writing to server.\n");
			h->iSendResult = send(*sockptr, "Done ", sizeof("Done ") + 1, 0);
			resetFileHandler(h);
		}
	}
	else {
		if (s < 0) { //something went wrong
			printf("%s %s error code %ld. Downloaded file is removed.\n", recipient, addr, s);
			fclose(h->f);
			remove(h->path.c_str());
			resetFileHandler(h);
		}
		else {
			h->canEnd = true;
			printf("%s replied with size %ld.\n", recipient, s);
			if (s < h->fbuflen) { //data lost- reduce buffer size
				h->index--;
				fseek(h->f, -1 * h->fbuflen, SEEK_CUR);
				h->fbuflen /= 2;
				h->dataDropped = true;
				h->canEnd = false;

			}
			else if (h->dataDropped) { //data was previously lost, but not this time- max buffer size found
				h->foundMax = true;
			}
			else if (!h->foundMax) { //no data lost yet, attempt to increase buffer size
				h->fbuflen *= 2;
			}
			if (h->fbuflen == 0) { //too much data has been lost, terminate transfer
				printf("Too much data lost, terminating. Any data Downloaded file will remain.");
				h->iSendResult = send(*sockptr, "-4", strlen("-4") + 1, 0);
				resetFileHandler(h);
			}
			else if (!h->finishNextLoop) {
				if (requestNewSize(h, h->fbuflen, h->fbuflen + 1 + getIndexSize(h->index), 1, false, sockptr)) {
					long long n = fread(h->fbuf, 1, h->fbuflen, h->f);
					int tmp = fflush(h->f);
					if (tmp != 0) printf("fflush: %d", tmp);
					bool end = false;
					if (n < h->fbuflen) {
						h->fbuflen = n;
						h->sendbuflen = h->fbuflen + 1 + getIndexSize(h->index);
						end = true;
					}
					if (h->fbuflen != 0) {
						sprintf(h->sendbuf, "%ld ", h->index);
						for (long long j = 0; j < h->fbuflen; j++) {
							*(h->sendbuf + j + 1 + getIndexSize(h->index)) = *(h->fbuf + j);
						}
						h->iSendResult = send(*sockptr, h->sendbuf, h->sendbuflen, 0);
						printf("Bytes TCP sent: %d; index: %ld; %s: %s\n", h->sendbuflen, h->index, recipient, addr);
					}
					else {
						printf("Done writing to %s %s.\n", recipient, addr);
						h->iSendResult = send(*sockptr, "Done ", sizeof("Done ") + 1, 0);
						resetFileHandler(h);
					}
					if (h->canEnd && end) {
						h->finishNextLoop = true;
					}
				}
				
			}
			else {
				printf("Done writing to %s %s.\n", recipient, addr);
				h->iSendResult = send(*sockptr, "Done ", sizeof("Done ") + 1, 0);
				resetFileHandler(h);
			}
		}
	}
	free(cmd);
}

void handleFileWrite(struct filehandler* h, SOCKET* sockptr, const char* recipient, const char* addr, long iResult_) {
	long s = strtol(h->recvbuf, NULL, 10);
	char* cmd;
	getCmd(&cmd, h->recvbuf);
	if (strcmp(cmd, "Done") == 0) { //All doen!!!!1!11!1
		printf("Done receiving from %s %s.\n", recipient, addr);
		fwrite(h->fbuf, 1, h->fbuflen, h->f);
		int tmp = fflush(h->f);
		if (tmp != 0) printf("fflush: %d", tmp);
		resetFileHandler(h);
	}
	else {
		long ind = strtol(cmd, NULL, 10);
		if (ind < 0) { //something went wrong
			printf("%s %s error code, terminating transfer %ld\n", recipient, addr, ind);
			fclose(h->f);
			resetFileHandler(h);
		}
		else if (ind == 0) { //index received was 0. Just skipping these, for some reason extra indexes get sent or something so yeah
			printf("Index received was 0, skipping rogue packet\n");
		}
		else {
			printf("Received index from %s %s: %ld; size: %d;\n", recipient, addr, ind, iResult_);
			long long l = iResult_ - (strlen(cmd) + 1);
			long long m = h->recvbuflen;
			bool updateBufs = false;
			if (ind + 1 == h->index) { //next set of data
				if (ind > 1) {
					fwrite(h->fbuf, 1, h->fbuflen, h->f);
					int tmp = fflush(h->f);
					if (tmp != 0) printf("fflush: %d", tmp);
				}
				if (h->dataDropped) {
					h->foundMax = true;
				}
				else if (!h->foundMax) {
					updateBufs = true;
					m = (l * 2) + 1 + getIndexSize(h->index);
				}
			}
			else { //data loss, or something else mysterious
				h->dataDropped = true;
				updateBufs = true;
				h->index--;
				m = ((h->recvbuflen - (1 + getIndexSize(h->index))) / 2) + 1 + getIndexSize(h->index);
			}
			if (requestNewSize(h, l, m, 2, false, sockptr)) {
				for (long long j = 0; j < h->fbuflen; j++) {
					*(h->fbuf + j) = *(h->recvbuf + j + (strlen(cmd) + 1));
				}
				sprintf(h->sendbuf, "%ld", h->fbuflen); //send back the length of the data retrieved to be written to the file
				h->iSendResult = send(*sockptr, h->sendbuf, strlen(h->sendbuf) + 1, 0);
				printf("Size sent: %s; %s: %s\n", h->sendbuf, recipient, addr);
			}
		}
	}
	free(cmd);
}
