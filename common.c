using namespace std;

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



