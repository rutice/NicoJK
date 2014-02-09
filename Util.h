#pragma once

#define FILETIME_MILLISECOND 10000LL

TCHAR *NewGetPrivateProfileSection(LPCTSTR lpAppName, LPCTSTR lpFileName);
void GetBufferedProfileString(LPCTSTR lpBuff, LPCTSTR lpKeyName, LPCTSTR lpDefault, LPTSTR lpReturnedString, DWORD nSize);
int GetBufferedProfileInt(LPCTSTR lpBuff, LPCTSTR lpKeyName, int nDefault);
BOOL WritePrivateProfileInt(LPCTSTR lpAppName, LPCTSTR lpKeyName, int value, LPCTSTR lpFileName);
DWORD GetLongModuleFileName(HMODULE hModule, LPTSTR lpFileName, DWORD nSize);
void AppendHttpHeader(char *str, const char *field, const char *value, const char *trail);
size_t FindHttpBody(const char *str);
bool HasToken(const char *str, const char *substr);
void DecodeEntityReference(TCHAR *str);
void EncodeEntityReference(const char *src, char *dest, int destSize);
COLORREF GetColor(const char *command);
bool GetChatDate(unsigned int *tm, const char *tag);
void UnixTimeToFileTime(unsigned int tm, FILETIME *pft);
unsigned int FileTimeToUnixTime(const FILETIME &ft);
FILETIME &operator+=(FILETIME &ft, LONGLONG offset);
LONGLONG operator-(const FILETIME &ft1, const FILETIME &ft2);
bool AribToSystemTime(const BYTE *pData, SYSTEMTIME *pst);
void GetFindFileList(LPCTSTR pattern, std::vector<WIN32_FIND_DATA> *pList, std::vector<LPWIN32_FIND_DATA> *pSortedList = NULL);
BOOL FileOpenDialog(HWND hwndOwner, LPCTSTR lpstrFilter, LPTSTR lpstrFile, DWORD nMaxFile);
bool ImportLogfile(LPCTSTR srcPath, LPCTSTR destPath, unsigned int tmNew);
bool GetProcessOutput(LPTSTR commandLine, LPCTSTR currentDir, char *buf, int bufSize, int timeout = INT_MAX);

struct LPWIN32_FIND_DATA_COMPARE {
	bool operator()(const WIN32_FIND_DATA *l, const WIN32_FIND_DATA *r) { return lstrcmpi(l->cFileName, r->cFileName) < 0; }
};

class CCriticalLock
{
public:
	CCriticalLock() { InitializeCriticalSection(&section_); }
	~CCriticalLock() { DeleteCriticalSection(&section_); }
	void Lock() { EnterCriticalSection(&section_); }
	void Unlock() { LeaveCriticalSection(&section_); }
	//CRITICAL_SECTION &GetCriticalSection() { return section_; }
private:
	CRITICAL_SECTION section_;
};

class CBlockLock
{
public:
	CBlockLock(CCriticalLock *pLock) : pLock_(pLock) { pLock_->Lock(); }
	~CBlockLock() { pLock_->Unlock(); }
private:
	CCriticalLock *pLock_;
};
