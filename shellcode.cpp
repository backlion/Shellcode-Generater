#include <windows.h>
#include <Winternl.h>
#pragma optimize( "", off ) 

BOOL __ISUPPER__(__in CHAR c);
CHAR __TOLOWER__(__in CHAR c);
UINT __STRLEN__(__in LPSTR lpStr1);
UINT __STRLENW__(__in LPWSTR lpStr1);
LPWSTR __STRSTRIW__(__in LPWSTR lpStr1, __in LPWSTR lpStr2);
INT __STRCMPI__(__in LPSTR lpStr1, __in LPSTR lpStr2);
INT __STRNCMPIW__(__in LPWSTR lpStr1, __in LPWSTR lpStr2, __in DWORD dwLen);
LPVOID __MEMCPY__(__in LPVOID lpDst, __in LPVOID lpSrc, __in DWORD dwCount);

FARPROC(WINAPI* GetProcAddressAPI)(HMODULE, LPCSTR);
HMODULE(WINAPI* LoadLibraryWAPI)(LPCWSTR);

inline BOOL __ISUPPER__(__in CHAR c) {
	return ('A' <= c) && (c <= 'Z');
};
inline CHAR __TOLOWER__(__in CHAR c) {
	return __ISUPPER__(c) ? c - 'A' + 'a' : c;
};

UINT __STRLEN__(__in LPSTR lpStr1)
{
	UINT i = 0;
	while (lpStr1[i] != 0x0)
		i++;

	return i;
}

UINT __STRLENW__(__in LPWSTR lpStr1)
{
	UINT i = 0;
	while (lpStr1[i] != L'\0')
		i++;

	return i;
}

LPWSTR __STRSTRIW__(__in LPWSTR lpStr1, __in LPWSTR lpStr2)
{
	CHAR c = __TOLOWER__(((PCHAR)(lpStr2++))[0]);
	if (!c)
		return lpStr1;

	UINT dwLen = __STRLENW__(lpStr2);
	do
	{
		CHAR sc;
		do
		{
			sc = __TOLOWER__(((PCHAR)(lpStr1)++)[0]);
			if (!sc)
				return NULL;
		} while (sc != c);
	} while (__STRNCMPIW__(lpStr1, lpStr2, dwLen) != 0);

	return (lpStr1 - 1); // FIXME -2 ?
}

INT __STRCMPI__(
	__in LPSTR lpStr1,
	__in LPSTR lpStr2)
{
	int  v;
	CHAR c1, c2;
	do
	{
		c1 = *lpStr1++;
		c2 = *lpStr2++;
		// The casts are necessary when pStr1 is shorter & char is signed 
		v = (UINT)__TOLOWER__(c1) - (UINT)__TOLOWER__(c2);
	} while ((v == 0) && (c1 != '\0') && (c2 != '\0'));
	return v;
}

INT __STRNCMPIW__(
	__in LPWSTR lpStr1,
	__in LPWSTR lpStr2,
	__in DWORD dwLen)
{
	int  v;
	CHAR c1, c2;
	do {
		dwLen--;
		c1 = ((PCHAR)lpStr1++)[0];
		c2 = ((PCHAR)lpStr2++)[0];
		/* The casts are necessary when pStr1 is shorter & char is signed */
		v = (UINT)__TOLOWER__(c1) - (UINT)__TOLOWER__(c2);
	} while ((v == 0) && (c1 != 0x0) && (c2 != 0x0) && dwLen > 0);

	return v;
}

LPSTR __STRCAT__(
	__in LPSTR	strDest,
	__in LPSTR strSource)
{
	LPSTR d = strDest;
	LPSTR s = strSource;

	while (*d) d++;

	do { *d++ = *s++; } while (*s);
	*d = 0x0;

	return strDest;
}

LPVOID __MEMCPY__(
	__in LPVOID lpDst,
	__in LPVOID lpSrc,
	__in DWORD dwCount)
{
	LPBYTE s = (LPBYTE)lpSrc;
	LPBYTE d = (LPBYTE)lpDst;

	while (dwCount--)
		*d++ = *s++;

	return lpDst;
}

HANDLE GetKernel32Handle() {
	HANDLE hKernel32 = INVALID_HANDLE_VALUE;
#ifdef _WIN64
	PPEB lpPeb = (PPEB)__readgsqword(0x60);
#else
	PPEB lpPeb = (PPEB)__readfsdword(0x30);
#endif
	PLIST_ENTRY pListHead = &lpPeb->Ldr->InMemoryOrderModuleList;
	PLIST_ENTRY pListEntry = pListHead->Flink;
	WCHAR strDllName[MAX_PATH];
	WCHAR strKernel32[] = { 'k', 'e', 'r', 'n', 'e', 'l', '3', '2', '.', 'd', 'l', 'l', L'\0' };

	while (pListEntry != pListHead) {
		PLDR_DATA_TABLE_ENTRY pModEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
		if (pModEntry->FullDllName.Length) {
			DWORD dwLen = pModEntry->FullDllName.Length;
			__MEMCPY__(strDllName, pModEntry->FullDllName.Buffer, dwLen);
			strDllName[dwLen / sizeof(WCHAR)] = L'\0';
			if (__STRSTRIW__(strDllName, strKernel32)) {
				hKernel32 = pModEntry->DllBase;
				break;
			}
		}
		pListEntry = pListEntry->Flink;
	}
	return hKernel32;
}

BOOL Initialize() {
	HANDLE hKernel32 = GetKernel32Handle();
	if (hKernel32 == INVALID_HANDLE_VALUE) {
		return FALSE;
	}
	LPBYTE lpBaseAddr = (LPBYTE)hKernel32;
	PIMAGE_DOS_HEADER lpDosHdr = (PIMAGE_DOS_HEADER)lpBaseAddr;
	PIMAGE_NT_HEADERS pNtHdrs = (PIMAGE_NT_HEADERS)(lpBaseAddr + lpDosHdr->e_lfanew);
	PIMAGE_EXPORT_DIRECTORY pExportDir = (PIMAGE_EXPORT_DIRECTORY)(lpBaseAddr + pNtHdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	LPDWORD pNameArray = (LPDWORD)(lpBaseAddr + pExportDir->AddressOfNames);
	LPDWORD pAddrArray = (LPDWORD)(lpBaseAddr + pExportDir->AddressOfFunctions);
	LPWORD pOrdArray = (LPWORD)(lpBaseAddr + pExportDir->AddressOfNameOrdinals);
	CHAR strLoadLibraryA[] = { 'L', 'o', 'a', 'd', 'L', 'i', 'b', 'r', 'a', 'r', 'y', 'W', 0x0 };
	CHAR strGetProcAddress[] = { 'G', 'e', 't', 'P', 'r', 'o', 'c', 'A', 'd', 'd', 'r', 'e', 's', 's', 0x0 };

	for (UINT i = 0; i < pExportDir->NumberOfNames; i++) {
		LPSTR pFuncName = (LPSTR)(lpBaseAddr + pNameArray[i]);
		if (!__STRCMPI__(pFuncName, strGetProcAddress)) {
			GetProcAddressAPI = (FARPROC(WINAPI*)(HMODULE, LPCSTR))(lpBaseAddr + pAddrArray[pOrdArray[i]]);
		}
		else if (!__STRCMPI__(pFuncName, strLoadLibraryA)) {
			LoadLibraryWAPI = (HMODULE(WINAPI*)(LPCWSTR))(lpBaseAddr + pAddrArray[pOrdArray[i]]);
		}
		if (GetProcAddressAPI != nullptr && LoadLibraryWAPI != nullptr) {
			return TRUE;
		}
	}
	return FALSE;
}


int main()
{
	Initialize();
	wchar_t struser32[] = { L'u', L's', L'e', L'r', L'3',L'2', L'.', L'd', L'l', L'l', 0 };
	char MeassageboxA_api[] = { 'M', 'e', 's', 's', 'a', 'g', 'e', 'B', 'o', 'x', 'A', 0 };

	using MESSAGEBOXA_INITIALIZE = UINT(WINAPI*)(HWND, LPCTSTR, LPCTSTR, UINT);
	MESSAGEBOXA_INITIALIZE MeassageboxA_MyOwn = reinterpret_cast<MESSAGEBOXA_INITIALIZE>(GetProcAddressAPI(LoadLibraryWAPI(struser32), MeassageboxA_api));
	MeassageboxA_MyOwn(NULL, NULL, NULL, 0);

	WCHAR strKernel32[] = { 'k', 'e', 'r', 'n', 'e', 'l', '3', '2', '.', 'd', 'l', 'l', L'\0' };
	char WinExec_api[] = { 'W', 'i', 'n', 'E', 'x', 'e', 'c', 0 };
	char strCalc[] = { 'c', 'a', 'l', 'c', '.', 'e', 'x', 'e', L'\0' };

	using WINEXEC_INITIALIZE = UINT(WINAPI*)(LPCSTR, UINT);
	WINEXEC_INITIALIZE WinExec_MyOwn = reinterpret_cast<WINEXEC_INITIALIZE>(GetProcAddressAPI(LoadLibraryWAPI(strKernel32), WinExec_api));
	WinExec_MyOwn(strCalc, 0);

	return 0;
}

