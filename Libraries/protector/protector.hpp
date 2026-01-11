#pragma once

#ifndef __PROTECOTR__H__
#define __PROTECOTR__H__

#include <iostream>
#include <windows.h>
#include <TlHelp32.h>
#include <tchar.h>
#include <thread>
#include <fstream>
#include "xorstr.hpp"
#include "bsod.h"
#include "MetaString.h"
#define STATUS_SUCCESS   ((NTSTATUS)0x00000000L) 

namespace protector {

	/* The more you increase the value, the later it will detect it, so adjust it carefully. */
	int scan_detection_time;

	/* Variables to enable or disable Protection Features. */
	bool scan_exe;
	bool scan_title;
	bool scan_driver;
	bool loop_killdbgr;

	bool debug_log;

	/* To activate the bsod function */
	bool protector_bsod;

	std::uint32_t find_dbg(const char* proc)
	{
		auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		auto pe = PROCESSENTRY32{ sizeof(PROCESSENTRY32) };

		if (Process32First(snapshot, &pe)) {
			do {
				if (!_stricmp(proc, pe.szExeFile)) {
					CloseHandle(snapshot);
					return pe.th32ProcessID;
				}
			} while (Process32Next(snapshot, &pe));
		}
		CloseHandle(snapshot);
		return 0;
	}
	
	/* The function that will run after the Debugger is detected. */
	/* You can add what you want, it's up to your imagination, I added the bsod function here. */
	void debugger_detected(std::string msg)
	{
		exit(0);
	}
	/* Basic Anti Debug Functions */
	void anti_dbg() {

		if (IsDebuggerPresent())
		{
			debugger_detected("IsDebuggerPresent");
		}
		else
		{
			
		}
	}

	/* Basic Anti Debug Functions */
	void anti_dbg_2() {
		
		__try {
			DebugBreak();
		}
		__except (GetExceptionCode() == EXCEPTION_BREAKPOINT ?
			EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
			
		}

	}


	/* Exe Detection Function */
	void exe_detect()
	{
		if (scan_exe == true) {

			if (find_dbg(XorStr("KsDumperClient.exe").c_str()))
			{
				debugger_detected(XorStr("KsDumper").c_str());
			}
			else if (find_dbg(XorStr("HTTPDebuggerUI.exe").c_str()))
			{
				debugger_detected(XorStr("HTTP Debugger").c_str());
			}
			else if (find_dbg(XorStr("HTTPDebuggerSvc.exe").c_str()))
			{
				debugger_detected(XorStr("HTTP Debugger Service"));
			}
			else if (find_dbg(XorStr("FolderChangesView.exe").c_str()))
			{
				debugger_detected(XorStr("FolderChangesView"));
			}
			else if (find_dbg(XorStr("ProcessHacker.exe").c_str()))
			{
				debugger_detected(XorStr("Process Hacker"));
			}
			else if (find_dbg(XorStr("procmon.exe").c_str()))
			{
				debugger_detected(XorStr("Process Monitor"));
			}
			else if (find_dbg(XorStr("idaq.exe").c_str()))
			{
				debugger_detected(XorStr("IDA"));
			}
			else if (find_dbg(XorStr("ida.exe").c_str()))
			{
				debugger_detected(XorStr("IDA"));
			}
			else if (find_dbg(XorStr("idaq64.exe").c_str()))
			{
				debugger_detected(XorStr("IDA"));
			}
			else if (find_dbg(XorStr("Wireshark.exe").c_str()))
			{
				debugger_detected(XorStr("WireShark").c_str());
			}
			else if (find_dbg(XorStr("Fiddler.exe").c_str()))
			{
				debugger_detected(XorStr("Fiddler").c_str());
			}
			else if (find_dbg(XorStr("Xenos64.exe").c_str()))
			{
				debugger_detected(XorStr("Xenos64").c_str());
			}
			else if (find_dbg(XorStr("Cheat Engine.exe").c_str()))
			{
				debugger_detected(XorStr("CheatEngine"));
			}
			else if (find_dbg(XorStr("HTTP Debugger Windows Service (32 bit).exe").c_str()))
			{
				debugger_detected(XorStr("HTTP Debugger"));
			}
			else if (find_dbg(XorStr("KsDumper.exe").c_str()))
			{
				debugger_detected(XorStr("KsDumper"));
			}
			else if (find_dbg(XorStr("x64dbg.exe").c_str()))
			{
				debugger_detected(XorStr("x64DBG"));
			}
			else if (find_dbg(XorStr("x64dbg.exe").c_str()))
			{
				debugger_detected(XorStr("x64DBG"));
			}
			else if (find_dbg(XorStr("x32dbg.exe").c_str()))
			{
				debugger_detected(XorStr("x32DBG"));
			}
			else if (find_dbg(XorStr("Fiddler Everywhere.exe").c_str()))
			{
				debugger_detected(XorStr("FiddlerEverywhere"));
			}
			else if (find_dbg(XorStr("die.exe").c_str()))
			{
				debugger_detected(XorStr("DetectItEasy"));
			}

			else if (find_dbg(XorStr("OLLYDBG.exe").c_str()))
			{
				debugger_detected(XorStr("OLLYDBG"));
			}

			else if (find_dbg(XorStr("HxD64.exe").c_str()))
			{
				debugger_detected(XorStr("HxD64"));
			}

			else if (find_dbg(XorStr("HxD32.exe").c_str()))
			{
				debugger_detected(XorStr("HxD64"));
			}

			else if (find_dbg(XorStr("snowman.exe").c_str()))
			{
				debugger_detected(XorStr("Snowman"));
			}

			else if (find_dbg(XorStr("tcpview.exe").c_str()))
			{
				debugger_detected(XorStr("tcpview"));
			}

			else if (find_dbg(XorStr("tcpview64.exe").c_str()))
			{
				debugger_detected(XorStr("tcpview"));
			}
			else if (find_dbg(XorStr("procexp.exe").c_str()))
			{
				debugger_detected(XorStr("Process Explorer"));
			}

		}
	}

	/* Title Detection Function */
	void title_detect()
	{
		if (scan_title == true) {

			HWND window;
			window = FindWindow(0, XorStr(("IDA: Quick start")).c_str());
			if (window)
			{
				debugger_detected(XorStr("IDA"));
			}


			window = FindWindow(0, XorStr(("Memory Viewer")).c_str());
			if (window)
			{
				debugger_detected(XorStr("CheatEngine"));
			}

			window = FindWindow(0, XorStr(("Cheat Engine")).c_str());
			if (window)
			{
				debugger_detected(XorStr("CheatEngine"));
			}

			window = FindWindow(0, XorStr(("Cheat Engine 7.4")).c_str());
			if (window)
			{
				debugger_detected(XorStr("CheatEngine"));
			}

			window = FindWindow(0, XorStr(("Cheat Engine 7.3")).c_str());
			if (window)
			{
				debugger_detected(XorStr("CheatEngine"));
			}

			window = FindWindow(0, XorStr(("Cheat Engine 7.2")).c_str());
			if (window)
			{
				debugger_detected(XorStr("CheatEngine"));
			}

			window = FindWindow(0, XorStr(("Cheat Engine 7.1")).c_str());
			if (window)
			{
				debugger_detected(XorStr("CheatEngine"));
			}

			window = FindWindow(0, XorStr(("Cheat Engine 7.0")).c_str());
			if (window)
			{
				debugger_detected(XorStr("CheatEngine"));
			}

			window = FindWindow(0, XorStr(("Process List")).c_str());
			if (window)
			{
				debugger_detected(XorStr("CheatEngine"));
			}

			window = FindWindow(0, XorStr(("x32DBG")).c_str());
			if (window)
			{
				debugger_detected(XorStr("x32DBG"));
			}

			window = FindWindow(0, XorStr(("x64DBG")).c_str());
			if (window)
			{
				debugger_detected(XorStr("x64DBG"));
			}

			window = FindWindow(0, XorStr(("KsDumper")).c_str());
			if (window)
			{
				debugger_detected(XorStr("KsDumper").c_str());
			}
			window = FindWindow(0, XorStr(("Fiddler Everywhere")).c_str());
			if (window)
			{
				debugger_detected(XorStr("FiddlerEverywhere"));
			}
			window = FindWindow(0, XorStr(("Fiddler Classic")).c_str());
			if (window)
			{
				debugger_detected(XorStr("FiddlerClassic"));
			}

			window = FindWindow(0, XorStr(("Fiddler Jam")).c_str());
			if (window)
			{
				debugger_detected(XorStr("FiddlerJam"));
			}

			window = FindWindow(0, XorStr(("FiddlerCap")).c_str());
			if (window)
			{
				debugger_detected(XorStr("FiddlerCap"));
			}

			window = FindWindow(0, XorStr(("FiddlerCore")).c_str());
			if (window)
			{
				debugger_detected(XorStr("FiddlerCore").c_str());
			}

			window = FindWindow(0, XorStr(("Scylla x86 v0.9.8")).c_str());
			if (window)
			{
				debugger_detected(XorStr("Scylla_x86").c_str());
			}

			window = FindWindow(0, XorStr(("Scylla x64 v0.9.8")).c_str());
			if (window)
			{
				debugger_detected(XorStr("Scylla_x64").c_str());
			}

			window = FindWindow(0, XorStr(("Scylla x86 v0.9.5a")).c_str());
			if (window)
			{
				debugger_detected(XorStr("Scylla_x86").c_str());
			}

			window = FindWindow(0, XorStr(("Scylla x64 v0.9.5a")).c_str());
			if (window)
			{
				debugger_detected(XorStr("Scylla_x64").c_str());
			}

			window = FindWindow(0, XorStr(("Scylla x86 v0.9.5")).c_str());
			if (window)
			{
				debugger_detected(XorStr("Scylla_x86").c_str());
			}

			window = FindWindow(0, XorStr(("Scylla x64 v0.9.5")).c_str());
			if (window)
			{
				debugger_detected(XorStr("Scylla_x64").c_str());
			}

			window = FindWindow(0, XorStr(("Detect It Easy v3.01")).c_str());
			if (window)
			{
				debugger_detected(XorStr("DetectItEasy").c_str());
			}

			window = FindWindow(0, XorStr(("Everything")).c_str());
			if (window)
			{
				debugger_detected(XorStr("Everything").c_str());
			}

			window = FindWindow(0, XorStr(("OllyDbg")).c_str());
			if (window)
			{
				debugger_detected(XorStr("OllyDbg"));
			}

			window = FindWindow(0, XorStr(("OllyDbg")).c_str());
			if (window)
			{
				debugger_detected(XorStr("OllyDbg"));
			}

			window = FindWindow(0, XorStr(("HxD")).c_str());
			if (window)
			{
				debugger_detected(XorStr("HxD"));
			}

			window = FindWindow(0, XorStr(("Snowman")).c_str());
			if (window)
			{
				debugger_detected(XorStr("HxD"));
			}

			window = FindWindow(0, XorStr(("TCPView")).c_str());
			if (window)
			{
				debugger_detected(XorStr("TCPView"));
			}
		}
	}




	/* Driver Detection Function */
	void driver_detect()
	{
		if (scan_driver == true) {

			const TCHAR* devices[] = {
		_T("\\\\.\\Dumper"),
		_T("\\\\.\\KsDumper")
			};

			WORD iLength = sizeof(devices) / sizeof(devices[0]);
			for (int i = 0; i < iLength; i++)
			{
				HANDLE hFile = CreateFile(devices[i], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				TCHAR msg[256] = _T("");
				if (hFile != INVALID_HANDLE_VALUE) {

					debugger_detected(XorStr("Driver Detected").c_str());

				}
				else
				{

				}
			}

		}
	}

	/* For Anti Dump */
	void EraseHeaderFromMemory()
	{

		DWORD oldProtect = 0;

		char* baseAdress = (char*)GetModuleHandle(NULL);

		// Change memory protection
		VirtualProtect(baseAdress, 4096,
			PAGE_READWRITE, &oldProtect);

		// Erase the header
		ZeroMemory(baseAdress, 4096);

	}

	BOOL ProcessDebugFlags()
	{

		/* Function Pointer Typedef for NtQueryInformationProcess */
		typedef NTSTATUS(WINAPI* pNtQueryInformationProcess)(IN  HANDLE, IN  UINT, OUT PVOID, IN ULONG, OUT PULONG);

		/* Created ProcessDebugFlags variable */
		const int ProcessDebugFlags = 0x1f;

		/* import the function */
		pNtQueryInformationProcess NtQueryInfoProcess = NULL;

		NTSTATUS Status;
		DWORD NoDebugInherit = 0;

		HMODULE hNtDll = LoadLibrary(TEXT(XorStr("ntdll.dll").c_str()));
		if (hNtDll == NULL)
		{

		}

		NtQueryInfoProcess = (pNtQueryInformationProcess)GetProcAddress(hNtDll, XorStr("NtQueryInformationProcess").c_str());

		if (NtQueryInfoProcess == NULL)
		{
			
		}

		// Call function
		Status = NtQueryInfoProcess(GetCurrentProcess(), ProcessDebugFlags, &NoDebugInherit, sizeof(DWORD), NULL);
		if (Status != 0x00000000)
			return false;
		if (NoDebugInherit == FALSE) {
			debugger_detected("ProcessDebugFlags");
			return true;
		}
		else {
			return false;
		}

	}

	BOOL SystemKernelDebuggerInformation()
	{

		typedef struct _SYSTEM_KERNEL_DEBUGGER_INFORMATION {
			BOOLEAN DebuggerEnabled;
			BOOLEAN DebuggerNotPresent;
		} SYSTEM_KERNEL_DEBUGGER_INFORMATION, * PSYSTEM_KERNEL_DEBUGGER_INFORMATION;
		SYSTEM_KERNEL_DEBUGGER_INFORMATION Info;

		enum SYSTEM_INFORMATION_CLASS { SystemKernelDebuggerInformation = 35 };

		/* Function Pointer Typedef for ZwQuerySystemInformation */
		typedef NTSTATUS(WINAPI* pZwQuerySystemInformation)(IN SYSTEM_INFORMATION_CLASS SystemInformationClass, IN OUT PVOID SystemInformation, IN ULONG SystemInformationLength, OUT PULONG ReturnLength);

		/* import the function */
		pZwQuerySystemInformation ZwQuerySystemInformation = NULL;

		HANDLE hProcess = GetCurrentProcess();

		HMODULE hNtDll = LoadLibrary(TEXT("ntdll.dll"));
		if (hNtDll == NULL)
		{
	
		}

		ZwQuerySystemInformation = (pZwQuerySystemInformation)GetProcAddress(hNtDll, XorStr("ZwQuerySystemInformation").c_str());

		if (ZwQuerySystemInformation == NULL)
		{

		}

		// Call function
		if (STATUS_SUCCESS == ZwQuerySystemInformation(SystemKernelDebuggerInformation, &Info, sizeof(Info), NULL)) {
			if (Info.DebuggerEnabled)
			{
				if (Info.DebuggerNotPresent) {
					return FALSE;
				}
				else {
					debugger_detected("SystemKernelDebuggerInformation");
					return TRUE;
				}
			}

		}
	}

	BOOL ThreadHideFromDebugger()
	{

		/* Function Pointer Typedef for NtQueryInformationProcess */
		typedef NTSTATUS(WINAPI* pNtSetInformationThread)(IN HANDLE, IN UINT, IN PVOID, IN ULONG);

		const int ThreadHideFromDebugger = 0x11;

		/* import the function */
		pNtSetInformationThread NtSetInformationThread = NULL;

		NTSTATUS Status;
		BOOL IsBeingDebug = FALSE;

		HMODULE hNtDll = LoadLibrary(TEXT("ntdll.dll"));
		if (hNtDll == NULL)
		{
	
		}

		NtSetInformationThread = (pNtSetInformationThread)GetProcAddress(hNtDll, "NtSetInformationThread");

		if (NtSetInformationThread == NULL)
		{
		
		}

		// Call Function
		Status = NtSetInformationThread(GetCurrentThread(), ThreadHideFromDebugger, NULL, 0);

		if (Status) {
			IsBeingDebug = TRUE;
			debugger_detected("ThreadHideFromDebugger");
		}

		return IsBeingDebug;
	}


	/* Start Protector Main Function */
	void protector()
	{
		/* We do it once. */
		//EraseHeaderFromMemory();
		while (true) {

			/* Protector Functions */
			protector::exe_detect();
			protector::title_detect();
			protector::driver_detect();
			protector::ProcessDebugFlags();
			protector::SystemKernelDebuggerInformation();
			protector::ThreadHideFromDebugger();

			/* Optimize (CPU) Required to reduce usage. */
			SleepEx(scan_detection_time, true);
		}
	}


	void start_protect() {

		/* Create threads for functions. */
		std::thread(protector).detach();
	}


};

#endif