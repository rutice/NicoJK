#include "stdafx.h"
#include "Util.h"

static const struct {
	COLORREF color;
	char *command;
} COMMAND2COLOR[] = {
	{RGB(0xFF, 0x00, 0x00), "red"},
	{RGB(0xFF, 0x80, 0x80), "pink"},
	{RGB(0xFF, 0xC0, 0x00), "orange"},
	{RGB(0xFF, 0xFF, 0x00), "yellow"},
	{RGB(0x00, 0xFF, 0x00), "green"},
	{RGB(0x00, 0xFF, 0xFF), "cyan"},
	{RGB(0x00, 0x00, 0xFF), "blue"},
	{RGB(0xC0, 0x00, 0xFF), "purple"},
	{RGB(0x00, 0x00, 0x00), "black"},
	{RGB(0xCC, 0xCC, 0x99), "white2"},
	{RGB(0xCC, 0xCC, 0x99), "niconicowhite"},
	{RGB(0xCC, 0x00, 0x33), "red2"},
	{RGB(0xCC, 0x00, 0x33), "truered"},
	{RGB(0xFF, 0x33, 0xCC), "pink2"},
	{RGB(0xFF, 0x66, 0x00), "orange2"},
	{RGB(0xFF, 0x66, 0x00), "passionorange"},
	{RGB(0x99, 0x99, 0x00), "yellow2"},
	{RGB(0x99, 0x99, 0x00), "madyellow"},
	{RGB(0x00, 0xCC, 0x66), "green2"},
	{RGB(0x00, 0xCC, 0x66), "elementalgreen"},
	{RGB(0x00, 0xCC, 0xCC), "cyan2"},
	{RGB(0x33, 0x99, 0xFF), "blue2"},
	{RGB(0x33, 0x99, 0xFF), "marineblue"},
	{RGB(0x66, 0x33, 0xCC), "purple2"},
	{RGB(0x66, 0x33, 0xCC), "nobleviolet"},
	{RGB(0x66, 0x66, 0x66), "black2"},
};

// 必要なバッファを確保してGetPrivateProfileSection()を呼ぶ
TCHAR *NewGetPrivateProfileSection(LPCTSTR lpAppName, LPCTSTR lpFileName)
{
	TCHAR *pBuf = NULL;
	for (int bufSize = 4096; bufSize < 1024 * 1024; bufSize *= 2) {
		delete [] pBuf;
		pBuf = new TCHAR[bufSize];
		if ((int)GetPrivateProfileSection(lpAppName, pBuf, bufSize, lpFileName) < bufSize - 2) {
			break;
		}
		pBuf[0] = 0;
	}
	return pBuf;
}

// GetPrivateProfileSection()で取得したバッファから、キーに対応する文字列を取得する
void GetBufferedProfileString(LPCTSTR lpBuff, LPCTSTR lpKeyName, LPCTSTR lpDefault, LPTSTR lpReturnedString, DWORD nSize)
{
	int nKeyLen = lstrlen(lpKeyName);
	if (nKeyLen <= 126) {
		TCHAR szKey[128];
		lstrcpy(szKey, lpKeyName);
		lstrcpy(szKey + (nKeyLen++), TEXT("="));
		while (*lpBuff) {
			int nLen = lstrlen(lpBuff);
			if (!StrCmpNI(lpBuff, szKey, nKeyLen)) {
				if ((lpBuff[nKeyLen] == TEXT('\'') || lpBuff[nKeyLen] == TEXT('"')) &&
				    nLen >= nKeyLen + 2 && lpBuff[nKeyLen] == lpBuff[nLen - 1]) {
					lstrcpyn(lpReturnedString, lpBuff + nKeyLen + 1, min(nLen-nKeyLen-1, static_cast<int>(nSize)));
				} else {
					lstrcpyn(lpReturnedString, lpBuff + nKeyLen, nSize);
				}
				return;
			}
			lpBuff += nLen + 1;
		}
	}
	lstrcpyn(lpReturnedString, lpDefault, nSize);
}

// GetPrivateProfileSection()で取得したバッファから、キーに対応する数値を取得する
int GetBufferedProfileInt(LPCTSTR lpBuff, LPCTSTR lpKeyName, int nDefault)
{
	TCHAR sz[24];
	GetBufferedProfileString(lpBuff, lpKeyName, TEXT(""), sz, _countof(sz));
	int nRet;
	return StrToIntEx(sz, STIF_DEFAULT, &nRet) ? nRet : nDefault;
}

BOOL WritePrivateProfileInt(LPCTSTR lpAppName, LPCTSTR lpKeyName, int value, LPCTSTR lpFileName)
{
	TCHAR sz[24];
	wsprintf(sz, TEXT("%d"), value);
	return WritePrivateProfileString(lpAppName, lpKeyName, sz, lpFileName);
}

DWORD GetLongModuleFileName(HMODULE hModule, LPTSTR lpFileName, DWORD nSize)
{
	TCHAR longOrShortName[MAX_PATH];
	DWORD nRet = GetModuleFileName(hModule, longOrShortName, MAX_PATH);
	if (nRet && nRet < MAX_PATH) {
		nRet = GetLongPathName(longOrShortName, lpFileName, nSize);
		if (nRet < nSize) return nRet;
	}
	return 0;
}

// HTTPヘッダフィールドを連結付加する
void AppendHttpHeader(char *str, const char *field, const char *value, const char *trail)
{
	// valueが空文字列なら何もしない
	if (value[0]) {
		int n = lstrlenA(str);
		lstrcatA(&str[n], field);
		lstrcatA(&str[n], value);
		lstrcatA(&str[n], trail);
	}
}

size_t FindHttpBody(const char *str)
{
	const char *p = strstr(str, "\r\n\r\n");
	return p ? p + 4 - str : strlen(str);
}

bool HasToken(const char *str, const char *substr)
{
	size_t len = strlen(substr);
	if (!strncmp(str, substr, len) && (!str[len] || str[len]==' ')) {
		return true;
	}
	for (; *str; ++str) {
		if (*str==' ' && !strncmp(str+1, substr, len) && (!str[1+len] || str[1+len]==' ')) {
			return true;
		}
	}
	return false;
}

void DecodeEntityReference(TCHAR *str)
{
	static const struct {
		TCHAR ent;
		LPCTSTR ref;
	} ENT_REF[] = {
		{TEXT('<'), TEXT("lt;")},
		{TEXT('>'), TEXT("gt;")},
		{TEXT('&'), TEXT("amp;")},
		{TEXT('"'), TEXT("quot;")},
		{TEXT('\''), TEXT("apos;")},
		{TEXT('\n'), TEXT("#10;")},
		{TEXT('\r'), TEXT("#13;")},
	};
	TCHAR *p = str;
	for (; *str; ++p) {
		if ((*p = *str++) == TEXT('&')) {
			for (int i = 0; i < _countof(ENT_REF); ++i) {
				int len = lstrlen(ENT_REF[i].ref);
				if (!StrCmpN(str, ENT_REF[i].ref, len)) {
					str += len;
					*p = ENT_REF[i].ent;
					break;
				}
			}
		}
	}
	*p = TEXT('\0');
}

void EncodeEntityReference(const char *src, char *dest, int destSize)
{
	// 切り捨てを防ぐには'&'に対して5倍のバッファを見積もる
	dest[0] = '\0';
	for (; *src; ++src) {
		char s[2] = {*src};
		const char *p = *s=='<' ? "&lt;" : *s=='>' ? "&gt;" : *s=='&' ? "&amp;" : s;
		if (lstrlenA(dest) + lstrlenA(p) >= destSize) {
			break;
		}
		lstrcatA(dest, p);
	}
}

COLORREF GetColor(const char *command)
{
	static const std::regex re("(?:^| )#([0-9A-Fa-f]{6})(?: |$)");
	std::cmatch m;
	if (std::regex_search(command, m, re)) {
		int color = strtol(m[1].first, NULL, 16);
		return RGB((color>>16)&0xFF, (color>>8)&0xFF, color&0xFF);
	}
	for (int i = 0; i < _countof(COMMAND2COLOR); ++i) {
		if (HasToken(command, COMMAND2COLOR[i].command)) {
			return COMMAND2COLOR[i].color;
		}
	}
	return RGB(0xFF, 0xFF, 0xFF);
}

bool GetChatDate(unsigned int *tm, const char *tag)
{
	// TODO: dateは秒精度しかないので独自に属性値つけるかvposを解釈するとよりよいかも
	static const std::regex re("^<chat[^>]*? date=\"(\\d+)\"");
	std::cmatch m;
	if (std::regex_search(tag, m, re)) {
		*tm = strtoul(m[1].first, NULL, 10);
		return true;
	}
	return false;
}

void UnixTimeToFileTime(unsigned int tm, FILETIME *pft)
{
	LONGLONG ll = static_cast<LONGLONG>(tm) * 10000000 + 116444736000000000;
	pft->dwLowDateTime = static_cast<DWORD>(ll);
	pft->dwHighDateTime = static_cast<DWORD>(ll >> 32);
}

unsigned int FileTimeToUnixTime(const FILETIME &ft)
{
	LONGLONG ll = (static_cast<LONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
	return static_cast<unsigned int>((ll - 116444736000000000) / 10000000);
}

FILETIME &operator+=(FILETIME &ft, LONGLONG offset)
{
	LONGLONG ll = (static_cast<LONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
	ll += offset;
	ft.dwLowDateTime = static_cast<DWORD>(ll);
	ft.dwHighDateTime = static_cast<DWORD>(ll >> 32);
	return ft;
}

LONGLONG operator-(const FILETIME &ft1, const FILETIME &ft2)
{
	LONGLONG ll1 = (static_cast<LONGLONG>(ft1.dwHighDateTime) << 32) | ft1.dwLowDateTime;
	LONGLONG ll2 = (static_cast<LONGLONG>(ft2.dwHighDateTime) << 32) | ft2.dwLowDateTime;
	return ll1 - ll2;
}

// 参考: ARIB STD-B10,TR-B13
static void SplitAribMjd(WORD wAribMjd, WORD *pwYear, WORD *pwMonth, WORD *pwDay, WORD *pwDayOfWeek)
{
	// MJD形式の日付を解析する
	DWORD dwYd = ((DWORD)wAribMjd * 20 - 301564) / 7305;
	DWORD dwMd = ((DWORD)wAribMjd * 10000 - 149561000 - dwYd * 1461 / 4 * 10000) / 306001;
	DWORD dwK = dwMd==14 || dwMd==15 ? 1 : 0;
	*pwDay = wAribMjd - 14956 - (WORD)(dwYd * 1461 / 4) - (WORD)(dwMd * 306001 / 10000);
	*pwYear = (WORD)(dwYd + dwK) + 1900;
	*pwMonth = (WORD)(dwMd - 1 - dwK * 12);
	*pwDayOfWeek = (wAribMjd + 3) % 7;
}

bool AribToSystemTime(const BYTE *pData, SYSTEMTIME *pst)
{
	if (pData[0]==0xFF && pData[1]==0xFF && pData[2]==0xFF && pData[3]==0xFF && pData[4]==0xFF) {
		// 不指定
		return false;
	}
	SplitAribMjd((pData[0]<<8)|pData[1], &pst->wYear, &pst->wMonth, &pst->wDay, &pst->wDayOfWeek);
	pst->wHour = (pData[2]>>4) * 10 + (pData[2]&0x0F);
	pst->wMinute = (pData[3]>>4) * 10 + (pData[3]&0x0F);
	pst->wSecond = (pData[4]>>4) * 10 + (pData[4]&0x0F);
	pst->wMilliseconds = 0;
	return true;
}

// FindFirstFile()の結果をstd::vectorで返す
void GetFindFileList(LPCTSTR pattern, std::vector<WIN32_FIND_DATA> *pList, std::vector<LPWIN32_FIND_DATA> *pSortedList)
{
	pList->clear();
	WIN32_FIND_DATA findData;
	HANDLE hFind = FindFirstFile(pattern, &findData);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			pList->push_back(findData);
		} while (FindNextFile(hFind, &findData));
		FindClose(hFind);
	}
	if (pSortedList) {
		pSortedList->clear();
		std::vector<WIN32_FIND_DATA>::iterator it = pList->begin();
		for (; it != pList->end(); ++it) {
			pSortedList->push_back(&(*it));
		}
		std::sort(pSortedList->begin(), pSortedList->end(), LPWIN32_FIND_DATA_COMPARE());
	}
}

// ファイルを開くダイアログ
BOOL FileOpenDialog(HWND hwndOwner, LPCTSTR lpstrFilter, LPTSTR lpstrFile, DWORD nMaxFile)
{
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwndOwner;
	ofn.lpstrFilter = lpstrFilter;
	ofn.lpstrTitle = TEXT("ファイルを開く");
	ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
	ofn.lpstrFile = lpstrFile;
	ofn.nMaxFile = nMaxFile;
	lpstrFile[0] = TEXT('\0');
	return GetOpenFileName(&ofn);
}

// ローカル形式をタイムシフトする
static bool TxtToLocalFormat(LPCTSTR srcPath, LPCTSTR destPath, unsigned int tmNew)
{
	FILE *fpDest = NULL;
	FILE *fpSrc;
	if (!lstrcmpi(PathFindExtension(srcPath), TEXT(".txt")) && !_tfopen_s(&fpSrc, srcPath, TEXT("r"))) {
		const std::regex re("^<chat[^>]*? date=\"(\\d+)\"");
		std::cmatch m;
		char buf[4096];
		unsigned int tmOld = 0;
		while (fgets(buf, _countof(buf), fpSrc)) {
			if (std::regex_search(buf, m, re)) {
				// chatタグが1行以上見つかれば書き込みを始める
				if (!fpDest && _tfopen_s(&fpDest, destPath, TEXT("w"))) {
					fpDest = NULL;
					break;
				}
				fwrite(buf, sizeof(char), m[1].first - buf, fpDest);
				unsigned int tm = strtoul(m[1].first, NULL, 10);
				if (!tmOld) {
					tmOld = tm;
				}
				fprintf(fpDest, "%u", !tmNew ? tm : tm - tmOld + tmNew);
				fputs(m[1].second, fpDest);
			}
		}
		fclose(fpSrc);
	}
	if (fpDest) {
		fclose(fpDest);
	}
	return fpDest != NULL;
}

static void WriteChatTag(FILE *fpDest, const std::cmatch &m, unsigned int *ptmOld, unsigned int tmNew)
{
	fwrite(m[0].first, sizeof(char), m[1].first - m[0].first, fpDest);
	unsigned int tm = strtoul(m[1].first, NULL, 10);
	if (!*ptmOld) {
		*ptmOld = tm;
	}
	fprintf(fpDest, "%u", !tmNew ? tm : tm - *ptmOld + tmNew);
	const char *p = m[1].second;
	int len = 0;
	for (; p + len < m[0].second; ++len) {
		// 改行文字は数値文字参照に置換
		if (p[len] == '\n' || p[len] == '\r') {
			fwrite(p, sizeof(char), len, fpDest);
			fprintf(fpDest, "&#%d;", p[len]);
			p += len + 1;
			len = -1;
		}
	}
	fwrite(p, sizeof(char), len, fpDest);
	fputs("\n", fpDest);
}

// JikkyoRec.jklをローカル形式に変換する
static bool JklToLocalFormat(LPCTSTR srcPath, LPCTSTR destPath, unsigned int tmNew)
{
	FILE *fpDest = NULL;
	FILE *fpSrc;
	if (!lstrcmpi(PathFindExtension(srcPath), TEXT(".jkl")) && !_tfopen_s(&fpSrc, srcPath, TEXT("rb"))) {
		char buf[4096];
		if (fread(buf, sizeof(char), 10, fpSrc) != 10 || memcmp(buf, "<JikkyoRec", 10) || _tfopen_s(&fpDest, destPath, TEXT("w"))) {
			fpDest = NULL;
		} else {
			// 空行まで読み飛ばす
			int c;
			for (int d = '\0'; (c = fgetc(fpSrc)) != EOF && !(d=='\n' && (c=='\n' || c=='\r')); d = c);

			const std::regex re("<chat[^>]*? date=\"(\\d+)\"[^]*?</chat>");
			std::cmatch m;
			int bufLen = 0;
			unsigned int tmOld = 0;
			while ((c = fgetc(fpSrc)) != EOF) {
				if (bufLen >= _countof(buf)) {
					bufLen = 0;
					continue;
				}
				buf[bufLen++] = static_cast<char>(c);
				if (c == '\0') {
					if (std::regex_search(buf, m, re)) {
						WriteChatTag(fpDest, m, &tmOld, tmNew);
					}
					bufLen = 0;
				}
			}
		}
		fclose(fpSrc);
	}
	if (fpDest) {
		fclose(fpDest);
	}
	return fpDest != NULL;
}

// ニコニコ実況コメントビューア.xmlをローカル形式に変換する
static bool XmlToLocalFormat(LPCTSTR srcPath, LPCTSTR destPath, unsigned int tmNew)
{
	FILE *fpDest = NULL;
	FILE *fpSrc;
	if (!lstrcmpi(PathFindExtension(srcPath), TEXT(".xml")) && !_tfopen_s(&fpSrc, srcPath, TEXT("r"))) {
		char buf[4096];
		if (!fgets(buf, _countof(buf), fpSrc) || !strstr(buf, "<?xml") || _tfopen_s(&fpDest, destPath, TEXT("w"))) {
			fpDest = NULL;
		} else {
			const std::regex re("<chat[^>]*? date=\"(\\d+)\"[^]*?</chat>");
			std::cmatch m;
			char tag[8192];
			tag[0] = '\0';
			unsigned int tmOld = 0;
			while (fgets(buf, _countof(buf), fpSrc)) {
				if (lstrlenA(buf) >= static_cast<int>(_countof(tag)) - lstrlenA(tag)) {
					tag[0] = '\0';
					continue;
				}
				lstrcatA(tag, buf);
				if (std::regex_search(tag, m, re)) {
					WriteChatTag(fpDest, m, &tmOld, tmNew);
					tag[0] = '\0';
				}
			}
		}
		fclose(fpSrc);
	}
	if (fpDest) {
		fclose(fpDest);
	}
	return fpDest != NULL;
}

bool ImportLogfile(LPCTSTR srcPath, LPCTSTR destPath, unsigned int tmNew)
{
	return JklToLocalFormat(srcPath, destPath, tmNew) ||
	       XmlToLocalFormat(srcPath, destPath, tmNew) ||
	       TxtToLocalFormat(srcPath, destPath, tmNew);
}

// 指定プロセスを実行して標準出力の文字列を得る
bool GetProcessOutput(LPTSTR commandLine, LPCTSTR currentDir, char *buf, int bufSize, int timeout)
{
	bool bRet = false;
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;
	HANDLE hReadPipe, hWritePipe;
	if (CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
		TCHAR lastDir[MAX_PATH];
		DWORD dwRet;
		if (!currentDir || (dwRet = GetCurrentDirectory(MAX_PATH, lastDir)) < MAX_PATH && dwRet && SetCurrentDirectory(currentDir)) {
			STARTUPINFO si = {0};
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdOutput = hWritePipe;
			PROCESS_INFORMATION pi;
			if (CreateProcess(NULL, commandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
				int bufCount = 0;
				bool bBreak = false;
				bRet = true;
				while (!bBreak) {
					timeout -= 100;
					if (WaitForSingleObject(pi.hProcess, 100) == WAIT_OBJECT_0) {
						bBreak = true;
					} else if (timeout <= 0) {
						bBreak = true;
						bRet = false;
					}
					DWORD avail;
					if (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &avail, NULL) && avail != 0) {
						if (bufCount + (int)avail >= bufSize) {
							bBreak = true;
							bRet = false;
						} else {
							DWORD read;
							if (ReadFile(hReadPipe, &buf[bufCount], avail, &read, NULL)) {
								bufCount += read;
							}
						}
					}
				}
				buf[bufCount] = '\0';
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
			}
			if (currentDir) {
				SetCurrentDirectory(lastDir);
			}
		}
		CloseHandle(hWritePipe);
		CloseHandle(hReadPipe);
	}
	return bRet;
}
