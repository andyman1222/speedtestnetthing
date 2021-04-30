#ifndef COMMON
#define COMMON
#undef UNICODE
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS true
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <string>
#include <list>
#include <signal.h>
#include <iostream>
#include <filesystem>
#include <chrono>

#pragma comment (lib, "Ws2_32.lib")
#define DEFAULT_BUFLEN 32
WSADATA wsaData;

int iResult;

#include "common.c"
#endif