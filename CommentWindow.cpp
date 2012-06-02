
#include "stdafx.h"
#include "CommentWindow.h"
#include "CommentPrinter.h"
#include "NicoJKSettings.h"
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ws2_32.lib")

// コメント取得用イベント
#define WMS_JKHOST_EVENT (WM_USER + 1)
#define WMS_GETFLV_EVENT (WM_USER + 2)
#define WMS_COMMENT_EVENT (WM_USER + 3)

#ifdef _DEBUG
#include <stdarg.h>
inline void dprintf_real( const _TCHAR * fmt, ... )
{
  _TCHAR buf[1024];
  va_list ap;
  va_start(ap, fmt);
  _vsntprintf_s(buf, 1024, fmt, ap);
  va_end(ap);
  OutputDebugString(buf);
}
#  define dprintf dprintf_real
#else
#  define dprintf __noop
#endif

struct COMMAND2COLOR {
	COLORREF color;
	wchar_t *command;
};

bool Cjk::doResize = false;
Cjk *Cjk::pSelf = NULL;

struct COMMAND2COLOR command2color[] = {
	{RGB(0xFF, 0x00, 0x00), L"red"},
	{RGB(0xFF, 0x80, 0x80), L"pink"},
	{RGB(0xFF, 0xC0, 0x00), L"orange"},
	{RGB(0xFF, 0xFF, 0x00), L"yellow"},
	{RGB(0x00, 0xFF, 0x00), L"green"},
	{RGB(0x00, 0xFF, 0xFF), L"cyan"},
	{RGB(0x00, 0x00, 0xFF), L"blue"},
	{RGB(0xC0, 0x00, 0xFF), L"purple"},
	{RGB(0x00, 0x00, 0x00), L"black"},
	{RGB(0xCC, 0xCC, 0x99), L"white2"},
	{RGB(0xCC, 0xCC, 0x99), L"niconicowhite"},
	{RGB(0xCC, 0x00, 0x33), L"red2"},
	{RGB(0xCC, 0x00, 0x33), L"truered"},
	{RGB(0xFF, 0x33, 0xCC), L"pink2"},
	{RGB(0xFF, 0x66, 0x00), L"orange2"},
	{RGB(0xFF, 0x66, 0x00), L"passionorange"},
	{RGB(0x99, 0x99, 0x00), L"yellow2"},
	{RGB(0x99, 0x99, 0x00), L"madyellow"},
	{RGB(0x00, 0xCC, 0x66), L"green2"},
	{RGB(0x00, 0xCC, 0x66), L"elementalgreen"},
	{RGB(0x00, 0xCC, 0xCC), L"cyan2"},
	{RGB(0x33, 0x99, 0xFF), L"blue2"},
	{RGB(0x33, 0x99, 0xFF), L"marineblue"},
	{RGB(0x66, 0x33, 0xCC), L"purple2"},
	{RGB(0x66, 0x33, 0xCC), L"nobleviolet"},
	{RGB(0x66, 0x66, 0x66), L"black2"}
};

#define COMMAND2COLOR_SIZE (sizeof(command2color)/sizeof(COMMAND2COLOR))
#define NO_COLOR	0xFFFFFFFF
COLORREF GetColor(const wchar_t *command) {
	if (command[0] == L'#') {
		if (wcslen(command) != 7) {
			return NO_COLOR;
		}
		int color = 0;
		for (int i=1; i<7; i++) {
			color *= 16;
			wchar_t c = towupper(command[i]);
			if (L'0' <= c && c <= L'9') {
				color += c - L'0';
			} else if (L'A' <= c && c <= L'F') {
				color += c - L'A' + 10;
			} else {
				return NO_COLOR;
			}
		}
		return RGB((color & 0xFF0000) >> 16, (color & 0x00FF00) >> 8, color & 0xFF);
	}
	for (int i=0; i<COMMAND2COLOR_SIZE; ++i) {
		if (wcscmp(command2color[i].command, command) == 0) {
			return command2color[i].color;
		}
	}
	return NO_COLOR;
}

class ChatManager {
	struct min_vpos_filter {
		int minvpos_;
		min_vpos_filter(int minvpos) : minvpos_(minvpos) {}
		bool operator()(const Chat& x) const { return x.vpos < minvpos_; }
	}; 

	int linecount_;
	int window_width_;
	std::vector<Chat> lines_;
	std::vector<Chat> linesShita_;
	std::vector<Chat> linesUe_;
	int iYPitch_;

	// Printer
	Printer *printer;

	// GDI
	RECT oldRect_;
	HWND hWnd_;
	HWND hNotify_;

	bool xmlGetText(const wchar_t *str, wchar_t *out, int len) {
		const wchar_t *es = wcschr(str, L'>');
		const wchar_t *ee = wcschr(str + 1, L'<');
		if (es && ee) {
			++es;
			wcsncpy_s(out, len, es, min(len - 1, ee - es));

			// entity reference
			wchar_t *p = wcschr(out, L'&');
			while(p) {
				if (wcsncmp(p, L"&lt;", 4) == 0) {
					*p = L'<';
					wcscpy_s(p+1, len - (p - out) - 1, p + 4);
				} else if (wcsncmp(p, L"&gt;", 4) == 0) {
					*p = L'>';
					wcscpy_s(p+1, len - (p - out) - 1, p + 4);
				} else if (wcsncmp(p, L"&amp;", 5) == 0) {
					*p = L'&';
					wcscpy_s(p+1, len - (p - out) - 1, p + 5);
				} else if (wcsncmp(p, L"&apos;", 6) == 0) {
					*p = L'"';
					wcscpy_s(p+1, len - (p - out) - 1, p + 6);
				}
				p = wcschr(p + 1, L'&');
			}
			return true;
		}
		return false;
	}

	// tokenはスペースで始まり=までつける 例：" mail=" or ATTR("mail")
#define ATTR(name) (L" " ##name L"=\"")
	bool xmlGetAttr(const wchar_t *in, const wchar_t *token, wchar_t *out, int len) {
		const wchar_t *start = wcsstr(in, token);
		if (start) {
			start += wcslen(token);
			const wchar_t *end = wcschr(start, L'"');
			if (end) {
				return wcsncpy_s(out, len, start, min(end - start, len - 1)) != EINVAL;
			}
		}
		return false;
	}
public:
	std::list<Chat> chat_;

	ChatManager() :
		linecount_(0),
		iYPitch_(0),
		hWnd_(NULL),
		hNotify_(NULL),
		printer(NULL)
	{
		SetRectEmpty(&oldRect_);
	}

	void Initialize(bool disableDWrite) {
		printer = CreatePrinter(disableDWrite);
	}

	bool insert(wchar_t *str, int vpos) {
		wchar_t szText[1024];
		wchar_t szCommand[1024] = L"";
		if (wcsncmp(str, L"<chat ", 6) == 0) {
			if (!xmlGetText(str, szText, 1024)) {
				return false;
			}
			
			Chat c(vpos, szText);
			if (xmlGetAttr(str, ATTR(L"mail"), szCommand, 1024)) {
				wchar_t *context = NULL;
				wchar_t *pt = wcstok_s(szCommand, L" ", &context);
				while (pt != NULL) {
					if (wcscmp(pt, L"184") == 0) {
						; // skip
					} else if (wcscmp(pt, L"shita") == 0) {
						c.position = CHAT_POS_SHITA;
					} else if (wcscmp(pt, L"ue") == 0) {
						c.position = CHAT_POS_UE;
					} else {
						COLORREF color = GetColor(pt);
						if (color != NO_COLOR) {
							c.color = color;
						}
					}
					pt = wcstok_s(NULL, L" ", &context);
				}
			}

			if (hWnd_) {
				UpdateChat(c);
				// コメントウィンドウにコメントを通知
			}
			if (hNotify_) {
				SendMessage(hNotify_, WM_NEWCOMMENT, 0L, (LPARAM)szText);
			}

			chat_.push_back(c);
		}
		return true;
	}

	bool trim(int minvpos) {
		chat_.remove_if(min_vpos_filter(minvpos));
		return true;
	}

	// コメントの表示ラインを設定する
	void UpdateChat(Chat &c) {
		if (c.position == CHAT_POS_SHITA) {
			int j;
			for (j=0; j<linecount_; j++) {
				if (linesShita_[j].vpos + VPOS_LEN < c.vpos) {
					break;
				}
			}
			if (j == linecount_) {
				j = 0;
			}
			c.line = static_cast<float>(j);
			linesShita_[j] = Chat(c);
			dprintf(L"%d = %d\n", j, c.vpos);
			return;
		} else if (c.position == CHAT_POS_UE) {
			// TODO: 上も対応・・・しないかも
		}

		SIZE size = printer->GetTextSize(c.text.c_str(), c.text.length());
		c.width = size.cx;

		int j;
		for (j=0; j<linecount_; j++) {
			Chat &chatj = lines_[j];
			// まだ前のが右端から出きってないとダメ
			if (chatj.vpos + 10 + VPOS_LEN * chatj.width / (window_width_ + chatj.width) > c.vpos) {
				continue;
			}
			// 前のに追いつかなければおｋ
			if (chatj.vpos + 10 + VPOS_LEN < c.vpos + VPOS_LEN * window_width_ / (window_width_ + c.width)) {
				break;
			}
		}
		c.line = (float)j;

		// 空きがなかったので適当にvposの小さいとこにずらして表示
		if (j == linecount_) {
			int vpos_min = 0x7fffffff;
			int line_index = 0;
			for (j=0; j<linecount_; j++) {
				if (lines_[j].vpos < vpos_min) {
					vpos_min = lines_[j].vpos;
					line_index = j;
				}
			}
			j = line_index;
			c.line = static_cast<float>(0.5 + j);
		}

		lines_[j] = Chat(c);
	}

	void Clear() {
		chat_.clear();
		linecount_ = -1;
		lines_.clear();
		linesShita_.clear();
		linesUe_.clear();
	}

	void PrepareChats(HWND hWnd) {
		if (!hWnd || !printer) {
			return;
		}

		RECT rc;
		GetClientRect(hWnd, &rc);
		if (EqualRect(&rc, &oldRect_)) {
			return;
		}
		CopyRect(&oldRect_, &rc);
		
		window_width_ = rc.right;
#if		0
		int yPitch = 30;
		int newLineCount = max(1, rc.bottom / yPitch - 1);
		// 適当に調整
		if (newLineCount > 12) {
			newLineCount = 12;
			yPitch = rc.bottom / (newLineCount + 1);
		} else if (newLineCount < 8) {
			newLineCount = 8;
			yPitch = rc.bottom / (newLineCount + 1);
		}
#endif
		int newLineCount = 12;
		int yPitch = rc.bottom / (newLineCount + 1);
		yPitch = (int)((float)yPitch * CNJIni::GetSettings()->commentSize);
		if (yPitch < CNJIni::GetSettings()->commentSizeMin) {
			yPitch = CNJIni::GetSettings()->commentSizeMin;
		}
		if (yPitch > CNJIni::GetSettings()->commentSizeMax) {
			yPitch = CNJIni::GetSettings()->commentSizeMax;
		}
		newLineCount = max(1, rc.bottom / (int)((float)yPitch * CNJIni::GetSettings()->commentLineMargin) - 1);
		printer->SetParams(hWnd, yPitch - 2);
		hWnd_ = hWnd;
		iYPitch_ = yPitch;

		linecount_ = newLineCount;
		lines_.assign(newLineCount, Chat());
		linesShita_.assign(newLineCount, Chat());
		linesUe_.assign(newLineCount, Chat());

		std::list<Chat>::iterator i;
		for (i=chat_.begin(); i!=chat_.end(); ++i) {
			UpdateChat(*i);
		}
	}
	
	void DrawComments(HWND hWnd, int vpos) {
		RECT rcWnd;
		GetClientRect(hWnd, &rcWnd);

		printer->Begin(rcWnd, iYPitch_);
		
		std::list<Chat>::const_iterator i = chat_.begin();
		for (; i!=chat_.cend(); ++i) {
			// skip head
			if (i->vpos < vpos - VPOS_LEN) {
				continue;
			}
			// end
			if (i->vpos > vpos) {
				break;
			}
			if (i->position == CHAT_POS_SHITA) {
				printer->DrawShita(*i);
			} else {
				printer->DrawNormal(*i, vpos);
			}
		}
		printer->End();
	}

	int GetYPitch() {
		return iYPitch_;
	}

	void SetNotifyWindow(HWND hWnd) {
		hNotify_ = hWnd;
	}
};
ChatManager manager;

Cjk::Cjk(TVTest::CTVTestApp *pApp, HWND hForce, bool disableDWrite) :
	m_pApp(pApp),
	hForce_(hForce),
	hWnd_(NULL),
	msPosition_(0)
{ 
	pSelf = this;
	manager.Initialize(disableDWrite);
}

Cjk::~Cjk()
{
	if ( pSelf == this )
	{
		pSelf = NULL;
	}
}

void Cjk::Create(HWND hParent) {
	static bool bInitialized = false;
	if (!bInitialized) {
		WNDCLASSEX wc;
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = NULL;
		wc.hIcon = NULL;
		wc.hCursor = NULL;
		wc.hbrBackground = CreateSolidBrush(COLOR_TRANSPARENT);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = TEXT("ru.jk");
		wc.hIconSm = NULL;

		RegisterClassEx(&wc);

		wc.lpfnWndProc = SocketProc;
		wc.hbrBackground = NULL;
		wc.lpszClassName = _T("ru.jk.socket");
		RegisterClassEx(&wc);

		hSocket_ = CreateWindowEx(
			WS_EX_NOACTIVATE,
			_T("ru.jk.socket"),
			_T("TvtPlayJK"),
			WS_POPUP,
			0, 0,
			1, 1,
			m_pApp->GetAppWindow(),
			NULL,
			NULL,
			(LPVOID)this);

		bInitialized = true;
	}

	if (m_pApp->GetFullscreen()) {
		hParent = GetFullscreenWindow();
	}

	hWnd_ = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_NOACTIVATE,
		(LPCTSTR)_T("ru.jk"),
		_T("NicoJK"),
		WS_POPUP | WS_VISIBLE,
		100, 100,
		400, 400,
		hParent,
		NULL,
		NULL,
		(LPVOID)this);

	HWND hButton = CreateWindow(
		_T("BUTTON"),
		_T("コメント再生開始"),
		WS_CHILD | BS_PUSHBUTTON,
		10, 10,
		200, 30,
		hWnd_,
		(HMENU)(INT_PTR)IDB_START,
		NULL,
		NULL
		);

	this->ResizeToVideoWindow();
}

void Cjk::Destroy() {
	DestroyWindow(hWnd_);
	hWnd_ = NULL;
}

void Cjk::DestroySocket() {
	if (socGetflv_ != INVALID_SOCKET) {
		shutdown(socGetflv_, SD_BOTH);
		closesocket(socComment_);
		socGetflv_ = INVALID_SOCKET;
	}
	if (socComment_ != INVALID_SOCKET) {
		shutdown(socComment_, SD_BOTH);
		closesocket(socComment_);
		socComment_ = INVALID_SOCKET;
	}
}

HWND Cjk::GetFullscreenWindow() {
	TVTest::HostInfo hostInfo;
	if (m_pApp->GetFullscreen() && m_pApp->GetHostInfo(&hostInfo)) {
		wchar_t className[64];
		::lstrcpynW(className, hostInfo.pszAppName, 48);
		::lstrcatW(className, L" Fullscreen");
		
		HWND hwnd = NULL;
		while ((hwnd = ::FindWindowEx(NULL, hwnd, className, NULL)) != NULL) {
			DWORD pid;
			::GetWindowThreadProcessId(hwnd, &pid);
			if (pid == ::GetCurrentProcessId()) return hwnd;
		}
	}
	return NULL;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd , LPARAM lp) {
	TCHAR strWindowText[1024];
	GetClassName(hwnd , strWindowText , 1024);
	if (wcsncmp(strWindowText, _T("TVTest View"), 1024)) return TRUE;
	RECT rc;
	GetWindowRect(hwnd, &rc);
	SetWindowPos(
		(HWND)lp,
		NULL,
		rc.left,
		rc.top,
		rc.right - rc.left,
		rc.bottom - rc.top,
		SWP_NOZORDER | SWP_NOACTIVATE);

	return FALSE;
}

void Cjk::ResizeToVideoWindow() {
	if (m_pApp->GetFullscreen()) {
		RECT rc;
		GetWindowRect(GetFullscreenWindow(), &rc);
		SetWindowPos(
			hWnd_,
			NULL,
			rc.left,
			rc.top,
			rc.right - rc.left,
			rc.bottom - rc.top,
			SWP_NOZORDER | SWP_NOACTIVATE);
	} else {
#if		0
		GetWindowRect(m_pApp->GetAppWindow(), &rc);
		SetWindowPos(
			hWnd_,
			NULL,
			rc.left + 10,
			rc.top + 10,
			rc.right - rc.left - 20,
			rc.bottom - rc.top - 60,
			SWP_NOZORDER | SWP_NOACTIVATE);
#endif
		EnumChildWindows(m_pApp->GetAppWindow(), EnumWindowsProc, (LPARAM)hWnd_);
	}
	manager.PrepareChats(hWnd_);
}

// tokenは ms= のように = もつける。
bool GetToken(const char *in, const char *token, char *out, int len) {
	const char *start = strstr(in, token);
	if (start) {
		start += strlen(token);
		const char *end = strchr(start, '&');
		if (end) {
			strncpy_s(out, len, start, min(end - start, len - 1));
			out[min(end - start, len - 1)] = '\0';
			return true;
		}
	}
	return false;
}

void Cjk::Open(int jkCh) {
	// ウィンドウを一旦壊す
	DestroySocket();
	manager.Clear();
	Destroy();
	Create(m_pApp->GetAppWindow());

	msPositionBase_ = 0;
	jkCh_ = jkCh;

	WSAAsyncGetHostByName(hSocket_, WMS_JKHOST_EVENT, "jk.nicovideo.jp", szHostbuf_, MAXGETHOSTSTRUCT);

	this->SetLiveMode();
	this->Start();
}

void Cjk::Start() {
	msPositionBase_ = msPosition_;
	ShowWindow(GetDlgItem(hWnd_, IDB_START), SW_HIDE);
}

void Cjk::SetLiveMode() {
	msSystime_ = 0;
	msPosition_ = timeGetTime();
}

void Cjk::SetPosition(int posms) {
	msSystime_ = timeGetTime();
	msPosition_ = posms;
}

void Cjk::DrawComments(HWND hWnd) {
	if (!msPositionBase_) {
		return;
	}

	int vpos = (msPosition_ - msPositionBase_ + (timeGetTime() - msSystime_)) / 10;
	manager.DrawComments(hWnd, vpos);
}

LRESULT CALLBACK Cjk::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	HDC hdc;
	PAINTSTRUCT ps;

	static Cjk *pThis = NULL;

	switch (msg) {
	case WM_CREATE:
		pThis = (Cjk*)((CREATESTRUCT*)lp)->lpCreateParams;
		SetTimer(hWnd, jkTimerID, CNJIni::GetSettings()->timerInterval, NULL);
		break;
	case WM_COMMAND:
		switch(LOWORD(wp)) {
		case IDB_START:
			pThis->Start();
			break;
		}
		break;
	case WM_SIZE:
		manager.PrepareChats(hWnd);
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		pThis->DrawComments(hWnd);
		EndPaint(hWnd, &ps);
		break;
	case WM_TIMER:
		switch ( wp )
		{
		case jkTimerID:
			if ( doResize && pSelf )
			{
				pSelf->ResizeToVideoWindow();
			}
			doResize = false;

			pThis->DrawComments(hWnd);
			//InvalidateRect(hWnd, NULL, FALSE);
			//UpdateWindow(hWnd);
			break;
		}
		break;
	case WM_CLOSE:
		KillTimer(hWnd, jkTimerID);
		KillTimer(hWnd, jkTimerResizeID);
		DestroyWindow(hWnd);
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_MOUSEMOVE:
		{
			POINT pt;
			RECT rc;

			pt.x = GET_X_LPARAM(lp);
			pt.y = GET_Y_LPARAM(lp);
			HWND hTarget = pThis->m_pApp->GetAppWindow();
			if (pThis->m_pApp->GetFullscreen()) {
				hTarget = pThis->GetFullscreenWindow();
			} 
			MapWindowPoints(hWnd, hTarget, &pt, 1);
			GetClientRect(hTarget,&rc);
			if (PtInRect(&rc,pt)) {
				return SendMessage(hTarget, msg, wp, MAKELPARAM(pt.x,pt.y));
			}
		}
		return 0;
	default:
		return (DefWindowProc(hWnd, msg, wp, lp));
	}
	return 0;
}

LRESULT CALLBACK Cjk::SocketProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	static Cjk *pThis = NULL;

	switch (msg) {
	case WM_CREATE:
		pThis = (Cjk*)((CREATESTRUCT*)lp)->lpCreateParams;
		pThis->socGetflv_ = INVALID_SOCKET;
		pThis->socComment_ = INVALID_SOCKET;
		manager.SetNotifyWindow(pThis->hForce_);
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WMS_JKHOST_EVENT:
		if (WSAGETASYNCERROR(lp) == 0) {
			HOSTENT *hostent = (HOSTENT*)pThis->szHostbuf_;
			OutputDebugString(_T("JKHOST_EVENT"));
			pThis->socGetflv_ = socket(PF_INET, SOCK_STREAM, 0);
			if (pThis->socGetflv_ == INVALID_SOCKET) {
				//error
			}
			WSAAsyncSelect(pThis->socGetflv_, hWnd, WMS_GETFLV_EVENT, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE);
			
			struct sockaddr_in serversockaddr;
			serversockaddr.sin_family = AF_INET;
			serversockaddr.sin_addr.s_addr = *(UINT*)(hostent->h_addr);
			serversockaddr.sin_port = htons(80);
			memset(serversockaddr.sin_zero, 0, sizeof(serversockaddr.sin_zero));
			if (connect(pThis->socGetflv_, (struct sockaddr*)&serversockaddr, sizeof(serversockaddr)) == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					MessageBox(hWnd, _T("ニコニコサーバへの接続に失敗しました。"), _T("Error"), MB_OK | MB_ICONERROR);
				}
			}
		}
		return TRUE;
	case WMS_GETFLV_EVENT:
		{
			SOCKET soc = (SOCKET)wp;
			char *szGetTemplate = "GET /api/getflv?v=jk%d HTTP/1.0\r\nHost: jk.nicovideo.jp\r\n\r\n";
			switch(WSAGETSELECTEVENT(lp)) {
			case FD_CONNECT:		/* サーバ接続したイベントの場合 */
				OutputDebugString(_T("getflv Connected"));
				break;
			case FD_WRITE:
				OutputDebugString(_T("getflv Write"));
				char szGet[1024];
				wsprintfA(szGet, szGetTemplate, pThis->jkCh_);
				send(soc, szGet, strlen(szGet)+1, 0);
				break;
			case FD_READ:
				char buf[10240];
				int read;
				read = recv(soc, buf, sizeof(buf)-1, 0);
				shutdown(soc, SD_BOTH);
				if (read == SOCKET_ERROR) {
					break;
				}
				buf[read] = '\0';
				if (strstr(buf, "done=true")) {
					char szMs[1024], szMsPort[1024];
					if (GetToken(buf, "ms=", szMs, 1024)
							&& GetToken(buf, "ms_port=", szMsPort, 1024)
							&& GetToken(buf, "thread_id=", pThis->szThread_, 1024)) {
						pThis->socComment_ = socket(PF_INET, SOCK_STREAM, 0);
						if (pThis->socComment_ == INVALID_SOCKET) {
							//error
						}
						WSAAsyncSelect(pThis->socComment_, hWnd, WMS_COMMENT_EVENT, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE);

						struct sockaddr_in serversockaddr;
						serversockaddr.sin_family = AF_INET;
						serversockaddr.sin_addr.s_addr = inet_addr(szMs);
						serversockaddr.sin_port = htons((unsigned short)atoi(szMsPort));
						memset(serversockaddr.sin_zero, 0, sizeof(serversockaddr.sin_zero));

						// コメントサーバに接続
						if (connect(pThis->socComment_, (struct sockaddr*)&serversockaddr, sizeof(serversockaddr)) == SOCKET_ERROR) {
							if (WSAGetLastError() != WSAEWOULDBLOCK) {
								MessageBox(hWnd, _T("コメントサーバへの接続に失敗しました。"), _T("Error"), MB_OK | MB_ICONERROR);
							}
						}
					}
				}
				OutputDebugStringA(buf);
				break;
			case FD_CLOSE:
				OutputDebugString(_T("getflv Closed"));
				closesocket(soc);
				pThis->socGetflv_ = INVALID_SOCKET;
				break;
			}
		}
		return TRUE;
	case WMS_COMMENT_EVENT:
		{
			SOCKET soc = (SOCKET)wp;
			const char *szRequestTemplate = "<thread res_from=\"-10\" version=\"20061206\" thread=\"%s\" />";
			switch(WSAGETSELECTEVENT(lp)) {
			case FD_CONNECT:		/* サーバ接続したイベントの場合 */
				OutputDebugString(_T("Comment Connected"));
				break;
			case FD_WRITE:
				OutputDebugString(_T("Comment Write"));
				char szRequest[1024];
				wsprintfA(szRequest, szRequestTemplate, pThis->szThread_);
				send(soc, szRequest, strlen(szRequest) + 1, 0); // '\0'まで送る
				break;
			case FD_READ:
				char buf[10240];
				int read;
				read = recv(soc, buf, sizeof(buf)-1, 0);
				if (read == SOCKET_ERROR) {
					break;
				}
				wchar_t wcsBuf[10240];
				int wcsLen;
				wcsLen = MultiByteToWideChar(CP_UTF8, 0, buf, read, wcsBuf, 10240);
				int pos;
				pos = 0;
				do {
					wchar_t *line = &wcsBuf[pos];
					int len = wcslen(line);
					manager.insert(line, timeGetTime() / 10);
					pos += len + 1;
				} while(pos < wcsLen);
				// 古いのは消す
				manager.trim(timeGetTime() / 10 - VPOS_LEN * 3);
				break;
			case FD_CLOSE:
				OutputDebugString(_T("Comment Closed"));
				closesocket(soc);
				pThis->socComment_ = INVALID_SOCKET;
				break;
			}
		}
		return TRUE;
	}
	return DefWindowProc(hWnd, msg, wp, lp);
}

BOOL Cjk::WindowMsgCallback(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam,LRESULT *pResult) {
	if (!hWnd_) {
		return FALSE;
	}

    switch (uMsg) {
	case WM_MOVE:
    case WM_SIZE:
		ResizeToVideoWindow();
		doResize = true;
		break;
	}
	return FALSE;
}

LRESULT Cjk::EventCallback(UINT Event, LPARAM lParam1, LPARAM lParam2) {
	if (!hWnd_) {
		return FALSE;
	}

	switch (Event) {
    case TVTest::EVENT_PLUGINENABLE:
        // プラグインの有効状態が変化した
        //return pThis->EnablePlugin(lParam1 != 0);
		break;
    case TVTest::EVENT_FULLSCREENCHANGE:
        // 全画面表示状態が変化した
        if (m_pApp->IsPluginEnabled()) {
			Destroy();
			Create(m_pApp->GetAppWindow());
		}
        break;
	}
	return 0;
}
