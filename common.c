using namespace std;

string TCPORT = "27015";

struct filehandler {
	//UNLIMITED POWWWWWWAR (well at least to 16^16, not taking into account 2s c)
	long long tempbuflen = DEFAULT_BUFLEN;
	//long long sendbuflen = DEFAULT_BUFLEN;
	long long buflen = DEFAULT_BUFLEN;
	long long _lastbuflen = buflen;
	//char* sendbuf = (char*)calloc(sendbuflen, 1);
	char* buf = (char*)calloc(buflen, 1);
	char* tempbuf = NULL;
	long index = 1; //biiiiiiiiiiiiig files
	int iSendResult = 0;
	int status = 0;
	int sendrecv = 0; //0 for send, 1 for recv
	bool foundMax = false;
	bool dataDropped = false;
	bool finishNextLoop = false, canEnd;
	bool keepLooping = true;
	FILE* f = NULL;
	long long fRemaining = -1;
	long long lastReadCount = 0;
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
			if (h->tempbuf) free(h->tempbuf);
			if (h->buf) free(h->buf);
			//if (h->sendbuf) free(h->sendbuf);
			h->tempbuf = NULL;
			h->buf = NULL;
			//h->sendbuf = NULL;
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
	h->lastReadCount = 0;
	h->foundMax = false;
	h->dataDropped = false;
	h->finishNextLoop = false;
	h->buflen = DEFAULT_BUFLEN;
	h->tempbuflen = DEFAULT_BUFLEN;
	//h->sendbuflen = DEFAULT_BUFLEN;
	if (h->buf) h->buf = (char*)realloc(h->buf, h->buflen);
	else h->buf = (char*)calloc(h->buflen, 1);
	//if (h->sendbuf) h->sendbuf = (char*)realloc(h->sendbuf, h->sendbuflen);
	//else h->sendbuf = (char*)calloc(h->sendbuflen, 1);
	if (!h->buf) {
		printf("Unable to allocate memory for buffer. aborting...");
		cleanupFileHandler(h);
	}
	//if (h->tempbuf) free(h->tempbuf);
	//h->tempbuf = NULL;
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
bool requestNewSize(struct filehandler* h, long long proposedBuf, bool abortOnFail = false, SOCKET* sockptr = NULL) {
	if (h->_lastbuflen != proposedBuf) {
		char* test = NULL;
		long long* bufreflen = NULL;
		char** bufref = NULL;
		h->buflen = proposedBuf;
		do {
			test = (char*)realloc(h->buf, h->buflen);
			if (!test) {
				h->foundMax = true;
				long long n = h->buflen / 2;
				long long diff = h->buflen - n;
				h->buflen = n;
				if (h->buflen <= 1) {
					printf("Allocated space for file buffer too small, unable to allocate.\n");
					if (!sockptr) h->iSendResult = send(*sockptr, "-4", strlen("-4") + 1, 0); //no file
					if (abortOnFail) cleanupFileHandler(h);
					return false;
				}
			}
			else h->buf = test;
		} while (!test);
		h->_lastbuflen = h->buflen;
	}
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
		fseek(h->f, 0L, SEEK_END);
		h->fRemaining = ftell(h->f);
		rewind(h->f);
		long long l = 1 + getIndexSize(h->index);
		h->buflen = DEFAULT_BUFLEN + l;
		bool end = false;
		if (h->buflen - l >= h->fRemaining) {
			h->buflen = h->fRemaining + l;
			end = true;
			requestNewSize(h, h->buflen, false, sockptr);
		}
		sprintf(h->buf, "%ld ", h->index);
		h->canEnd = true;
		long long n = fread(h->buf + l, 1, h->buflen - l, h->f);
		int tmp = fflush(h->f);
		if (tmp != 0) printf("fflush: %d", tmp);
		h->lastReadCount = n;
		h->fRemaining -= n;
		if (n != 0) {
			h->iSendResult = send(*sockptr, h->buf, h->buflen, 0);
			printf("Initial bytes TCP sent: %d; index: %ld; %s: %s\n", h->buflen, h->index, recipient, addr);
			h->index++;
			if (h->canEnd && end) {
				h->finishNextLoop = true;
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
		h->buflen = DEFAULT_BUFLEN + 1 + getIndexSize(h->index);
		h->buf = (char*)realloc(h->buf, h->buflen); //account for ulong, a space, fbuf
		if (!h->buf) {
			printf("Unable to allocate receiving buffer memory, aborting.");
			cleanupFileHandler(h);
		}
		else {
			h->tempbuflen = DEFAULT_BUFLEN;
			if (h->tempbuf) h->tempbuf = (char*)realloc(h->tempbuf, h->tempbuflen);
			else h->tempbuf = (char*)calloc(h->tempbuflen, 1);
			if (!h->tempbuf) {
				printf("Unable to allocate receiving buffer memory, aborting.");
				cleanupFileHandler(h);
			}
			else {
				h->iSendResult = send(*sockptr, "ready ", strlen("ready ") + 1, 0);
				printf("Starting file retrieval with %s %s\n", recipient, addr);
			}
		}
	}
}

void handleFileRead(struct filehandler* h, SOCKET* sockptr, const char* recipient, const char* addr) {
	long s = strtol(h->buf, NULL, 10);
	char* cmd;
	getCmd(&cmd, h->buf);
	long long g = 1 + getIndexSize(h->index);
	if (strcmp(cmd, "ready") == 0) { //ready 2 send data!!!1!1 client only
		requestNewSize(h, DEFAULT_BUFLEN + g, false, sockptr);
		sprintf(h->buf, "%ld ", h->index);
		bool end = false;
		if (h->buflen - g >= h->fRemaining) {
			h->buflen = h->fRemaining + g;
			end = true;
			requestNewSize(h, h->buflen, false, sockptr);
		}
		long long m = fread(h->buf + g, 1, h->buflen - g, h->f);
		int tmp = fflush(h->f);
		if (tmp != 0) printf("fflush: %d", tmp);
		h->canEnd = true;
		h->lastReadCount = m;
		h->fRemaining -= m;
		if (m != 0) {
			h->iSendResult = send(*sockptr, h->buf, h->buflen, 0);
			printf("Initial bytes TCP sent: %d; index: %ld;\n", h->buflen, h->index);
			h->index++;
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
			printf("%s replied with size %ld, expected %ld.\n", recipient, s, h->lastReadCount);
			if (s < h->lastReadCount) { //data lost- reduce buffer size
				h->index--;
				fseek(h->f, -1 * (h->lastReadCount), SEEK_CUR);
				h->fRemaining += h->lastReadCount;
				h->buflen = ((h->buflen - g) / 2) + g;
				h->dataDropped = true;
				h->canEnd = false;

			}
			else if (h->dataDropped) { //data was previously lost, but not this time- max buffer size found
				h->foundMax = true;
			}
			else if (!h->foundMax) { //no data lost yet, attempt to increase buffer size
				h->buflen = ((h->buflen - g) * 2) + g;
			}
			if (h->buflen - g <= 1) { //too much data has been lost, terminate transfer
				printf("Too much data lost, terminating. Any data Downloaded file will remain.");
				h->iSendResult = send(*sockptr, "-4", strlen("-4") + 1, 0);
				resetFileHandler(h);
			}
			else if (!h->finishNextLoop) {
				bool end = false;
				if (h->buflen - g >= h->fRemaining) {
					h->buflen = h->fRemaining + g;
					end = true;
				}
				if (requestNewSize(h, h->buflen, false, sockptr)) {
					sprintf(h->buf, "%ld ", h->index);

					long long n = fread(h->buf + g, 1, h->buflen - g, h->f);
					int tmp = fflush(h->f);
					if (tmp != 0) printf("fflush: %d", tmp);
					h->lastReadCount = n;
					h->fRemaining -= n;
					if (n != 0) {
						h->iSendResult = send(*sockptr, h->buf, h->buflen, 0);
						printf("Bytes TCP sent: %d; index: %ld; %s: %s\n", h->buflen, h->index, recipient, addr);
						h->index++;
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
	long s = strtol(h->buf, NULL, 10);
	char* cmd;
	getCmd(&cmd, h->buf);
	if (strcmp(cmd, "Done") == 0) { //All doen!!!!1!11!1
		printf("Done receiving from %s %s.\n", recipient, addr);
		fwrite(h->tempbuf, 1, h->tempbuflen, h->f);
		int tmp = fflush(h->f);
		if (tmp != 0) printf("fflush: %d", tmp);
		free(h->tempbuf);
		h->tempbuf = NULL;
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
			long long g = strlen(cmd) + 1;
			long long l = iResult_ - g;
			bool updateBufs = false;
			bool alreadyReducedIndex = false;
			if (ind == h->index) { //next set of data
				if (ind > 1) {
					fwrite(h->tempbuf, 1, h->tempbuflen, h->f);

					int tmp = fflush(h->f);
					if (tmp != 0) printf("fflush: %d", tmp);
				}
				if (h->dataDropped) {
					h->foundMax = true;
				}
				else if (!h->foundMax) {
					updateBufs = true;
					h->buflen = (l * 2) + 1 + getIndexSize(h->index + 1);
				}
			}
			else { //data loss, or something else mysterious
				h->dataDropped = true;
				updateBufs = true;
				h->index--;
				alreadyReducedIndex = true;
				h->buflen = iResult_;
			}
			if (requestNewSize(h, h->buflen, false, sockptr)) {
				char tmp[DEFAULT_BUFLEN];
				sprintf(tmp, "%ld", l); //send back the length of the data retrieved to be written to the file
				if (l != h->tempbuflen) {
					long long prevtempbuflen = h->tempbuflen;
					h->tempbuflen = l;
					bool good = true;
					char* test = (char*)realloc(h->tempbuf, h->tempbuflen);
					if (!test) {
						h->foundMax = true;
						h->tempbuflen = prevtempbuflen;
						if (l > h->tempbuflen) {
							if(!alreadyReducedIndex) h->index--;
							h->dataDropped = true;
						}
					}
					else h->tempbuf = test;
				}
				memcpy(h->tempbuf, h->buf + (g), l);
				h->iSendResult = send(*sockptr, tmp, strlen(tmp) + 1, 0);
				printf("Size sent: %s; %s: %s\n", tmp, recipient, addr);
				h->index++;
			}
		}
	}
	free(cmd);
}
