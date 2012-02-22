// NicoJK.cpp
//
/*
	NicoJK
		TVTest ニコニコ実況プラグイン
*/

#include "stdafx.h"
#include "NicoJK.h"
#include "resource.h"
#include "CommentWindow.h"

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

// static変数
CNicoJK *CNicoJK::this_;

bool CNicoJK::GetPluginInfo(TVTest::PluginInfo *pInfo) {
	// プラグインの情報を返す
	pInfo->Type           = TVTest::PLUGIN_TYPE_NORMAL;
	pInfo->Flags          = 0;
	pInfo->pszPluginName  = L"NicoJK";
	pInfo->pszCopyright   = L"Public Domain";
	pInfo->pszDescription = L"ニコニコ実況をSDKで表示";
	return true;
}

bool CNicoJK::Initialize() {
	// 初期化処理
	if (GetModuleFileName(g_hinstDLL, szIniFileName_, _countof(szIniFileName_))) {
		PathRenameExtension(szIniFileName_, TEXT(".ini"));
	}

	// staticコールバック用
	this_ = this;

	isJK = false;

	// 重ねるモード
	cwMode_ = 1;
	if (GetPrivateProfileInt(_T("Setting"), _T("useSDKAttach"), 0, szIniFileName_)) {
		cwMode_ = 0;
	}

	INITCOMMONCONTROLSEX ic;
	ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
	ic.dwICC = ICC_TAB_CLASSES;
	InitCommonControlsEx(&ic);

	CoInitialize(NULL);
	WSADATA wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);
	hGethost_ = NULL;
	socJkapi_ = INVALID_SOCKET;

	// 勢い窓作成
	hForce_ = CreateDialogParam(g_hinstDLL, MAKEINTRESOURCE(IDD_FORCE),
									 NULL, (DLGPROC)ForceDialogProc, (LPARAM)this);

	// 独自実装
	useSDK_ = 0 != GetPrivateProfileInt(_T("Setting"), _T("useSDK"), 0, szIniFileName_);
	jkcw_ = new Cjk(m_pApp, hForce_);

	// イベントコールバック関数を登録
	m_pApp->SetEventCallback(EventCallback, this);
	// ウィンドウコールバック関数を登録
	m_pApp->SetWindowMessageCallback(WindowMsgCallback, this);

	return true;
}

bool CNicoJK::Finalize() {
	// 終了処理
	StopJK();

	SendMessage(hForce_, WM_CLOSE, 0L, 0L);
	DestroyWindow(hForce_);

	delete jkcw_;

	WSACleanup();
	CoUninitialize();
	return true;
}

void CNicoJK::TogglePlugin(bool bEnabled) {
	if (bEnabled) {
		OnChannelChange();
		PostMessage(hForce_, WM_TIMER, TIMER_UPDATE, 0L);
		ShowWindow(hForce_, SW_SHOW);
	} else {
		ShowWindow(hForce_, SW_HIDE);
		StopJK();
	}
}

void CNicoJK::StartJK(int jkID) {
	StopJK();

	if (jkID > 100 || !useSDK_) {
		jkcw_->Open(jkID);
		return;
	}

	HRESULT hr;
	jk_.CreateInstance(CLSID_JKNiCOM);
	if (!jk_) {
		MessageBox(NULL, _T("ニコニコ実況SDKが入ってないかも。"), NULL, MB_OK);
		return;
	}

	isJK = true;

	long val;
	jk_->GetCurrentVersion(&val);
	if (val < 110) {
		MessageBox(NULL, _T("ニコニコ実況SDKが古いかも。"), NULL, MB_OK);
		jk_ = NULL;
		return;
	}

	jk_->get_Channels(&channels_);
	if (!channels_) {
		MessageBox(NULL, _T("チャンネル一覧がとれませんでした。"), NULL, MB_OK);
		jk_ = NULL;		
		return ;
	}

	long count;
	channels_->get_Count(&count);
	for(int i=0; i<count; i++) {
		IChannelPtr c;
		channels_->Item(i, &c);
		long num;
		c->get_Number(&num);

		if (num == jkID) {
			channel_ = c;
			break;
		}
	}
	
	if (!channel_) {
		return;
	}

	jk_->get_CommentWindow(&cw_);
	cw_->put_Visible(VARIANT_FALSE);

	HWND target = NULL;
	if (m_pApp->GetFullscreen()) {
		target = GetFullscreenWindow();
	}
	if (!target) {
		target = m_pApp->GetAppWindow();
	}
	
	cw_->put_Transparent(VARIANT_TRUE);
	if (cwMode_) {
		cw_->Start(channel_, &hr);
		cw_->put_TopMost(VARIANT_TRUE);
		cw_->put_Visible(VARIANT_TRUE);
		AdjustCommentWindow();
	} else {
		cw_->AttachWindowByHandle(HandleToLong(target), &hr);
		cw_->Start(channel_, &hr);
	}
}

void CNicoJK::StopJK() {
	if (!isJK && !useSDK_) {
		jkcw_->Destroy();
		return;
	}

	if (isJK == false) {
		return;
	}
	if (cw_) {
		cw_->DetachWindow();
		HRESULT hr;
		cw_->Stop(&hr);
		cw_ = NULL;
	}
	
	channel_ = NULL;
	channels_ = NULL;
	jk_ = NULL;
}

void CNicoJK::AdjustCommentWindow() {
	if (cw_) {
		HWND target = NULL;
		if (m_pApp->GetFullscreen()) {
			target = GetFullscreenWindow();
			HRESULT hr;
			cw_->AttachWindowByHandle(HandleToLong(target), &hr);
			return ;
		}
		RECT rc;
		GetWindowRect(m_pApp->GetAppWindow(), &rc);
		cw_->put_X(rc.left + 10);
		cw_->put_Y(rc.top + 22);
		cw_->put_Width(rc.right - rc.left - 20);
		cw_->put_Height(rc.bottom - rc.top - 44);
	}
}

HWND CNicoJK::GetFullscreenWindow() {
	TVTest::HostInfo hostInfo;
	if (m_pApp->GetFullscreen() && m_pApp->GetHostInfo(&hostInfo)) {
		wchar_t className[64];
		lstrcpynW(className, hostInfo.pszAppName, 48);
		lstrcatW(className, L" Fullscreen");

		HWND hwnd = NULL;
		while ((hwnd = FindWindowExW(NULL, hwnd, className, NULL)) != NULL) {
			DWORD pid;
			GetWindowThreadProcessId(hwnd, &pid);
			if (pid == GetCurrentProcessId()) {
				return hwnd;
			}
		}
	}
	return NULL;
}

int CNicoJK::GetJKByChannelName(const wchar_t *name) {
	int jkID = GetPrivateProfileInt(_T("Channels"), name, -1, szIniFileName_);
	if (jkID == -1) {
		jkID = GetPrivateProfileInt(_T("Setting"), name, -1, szIniFileName_);
	}
	return jkID;
}

void CNicoJK::OnChannelChange() {
	TVTest::ChannelInfo info;
	m_pApp->GetCurrentChannelInfo(&info);
	OutputDebugStringW(info.szChannelName);

	int jkID = GetJKByChannelName(info.szChannelName);
	ForceDialog_UpdateForce();
	ListBox_ResetContent(GetDlgItem(hForce_, IDC_LOG));

	if (jkID != -1) {
		StartJK(jkID);
	} else {
		StopJK();
	}
}

void CNicoJK::OnFullScreenChange() {
	HRESULT hr;
	if (cw_) {
		if (cwMode_) {
			AdjustCommentWindow();
		} else {
			if (m_pApp->GetFullscreen()) {
				HWND hFull = GetFullscreenWindow();
				cw_->AttachWindowByHandle(HandleToLong(hFull), &hr);
				return ;
			}
			cw_->AttachWindowByHandle(HandleToLong(m_pApp->GetAppWindow()), &hr);
		}
	}
}

// チャンネル切り替えから実況切り替えまでのタイマ
VOID CALLBACK CNicoJK::OnServiceChangeTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	if (this_) {
		KillTimer(NULL, this_->timerID_);
		this_->timerID_ = NULL;
		this_->OnChannelChange();
	}
}

// イベントコールバック関数
// 何かイベントが起きると呼ばれる
LRESULT CALLBACK CNicoJK::EventCallback(UINT Event,LPARAM lParam1,LPARAM lParam2,void *pClientData)
{
	CNicoJK *pThis = static_cast<CNicoJK*>(pClientData);
	if (pThis->useSDK_ == 0 && pThis->jkcw_) {
		pThis->jkcw_->EventCallback(Event, lParam1, lParam2);
	}
	switch (Event) {
	case TVTest::EVENT_PLUGINENABLE:
		pThis->TogglePlugin(lParam1 != 0);
		return TRUE;
	case TVTest::EVENT_FULLSCREENCHANGE:
		if (pThis->m_pApp->IsPluginEnabled()) {
			pThis->OnFullScreenChange();
		}
		return TRUE;
	case TVTest::EVENT_DRIVERCHANGE:
		if (pThis->m_pApp->IsPluginEnabled()) {
			pThis->StopJK();
		}
		return TRUE;
	case TVTest::EVENT_CHANNELCHANGE:
	case TVTest::EVENT_SERVICECHANGE:
		if (pThis->m_pApp->IsPluginEnabled()) {
			if (pThis->timerID_) {
				KillTimer(NULL, pThis->timerID_);
			}
			pThis->timerID_ = SetTimer(NULL, 0, 750, pThis->OnServiceChangeTimer);
		}
		return TRUE;
	}
	return 0;
}

BOOL CALLBACK CNicoJK::WindowMsgCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *pResult, void *pUserData)
{
	CNicoJK *pThis = static_cast<CNicoJK*>(pUserData);
	if (pThis->useSDK_ == 0 && pThis->jkcw_) {
		pThis->jkcw_->WindowMsgCallback(hwnd, uMsg, wParam, lParam, pResult);
	}
	switch (uMsg) {
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			if (pThis->m_pApp->IsPluginEnabled()) {
				if (pThis->m_pApp->GetFullscreen()) {
					pThis->AdjustCommentWindow();
					return TRUE;
				}
			}
		} else {
			if (pThis->m_pApp->IsPluginEnabled()) {
				SetWindowPos(pThis->hForce_, pThis->m_pApp->GetAppWindow(), 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE);
			}
		}
		break;
	case WM_MOVE:
	case WM_SIZE:
		if (pThis->m_pApp->IsPluginEnabled()) {
			pThis->AdjustCommentWindow();
		}
		break;
	}
	return FALSE;
}

// 勢い窓関連
BOOL CNicoJK::ForceDialog_UpdateForce() {
	if (!useSDK_) {
		if (socJkapi_ != INVALID_SOCKET) {
			shutdown(socJkapi_, 1);
			closesocket(socJkapi_);
		}
		if (!hGethost_) {
			hGethost_ = WSAAsyncGetHostByName(hForce_, WMS_APIJK_HOST, "api.jk.nicovideo.jp", szHostbuf_, MAXGETHOSTSTRUCT);
		}
		return TRUE;
	}

	HWND hList = GetDlgItem(hForce_, IDC_FORCELIST);
	ListBox_ResetContent(hList);

	IChannelCollectionPtr cc;
	if (jk_) {
		jk_->get_Channels(&cc);
	} else {
		IJKNiCOMPtr jk;
		jk.CreateInstance(CLSID_JKNiCOM);
		jk->get_Channels(&cc);
	}
	if (cc) {
		long count;
		cc->get_Count(&count);
		for (long i=0; i<count; i++) {
			IChannelPtr c;
			cc->Item(i, &c);
			BSTR chName;
			c->get_Name(&chName);
			long jkVal;
			c->get_Number(&jkVal);
			long force;
			c->get_ThreadForce(&force);

			wchar_t sz[1024];
			wsprintfW(sz, L"jk%d (%s) 勢い：%d", jkVal, chName, force);
			ListBox_AddString(hList, sz);
			SysFreeString(chName);
		}
	}
	return TRUE;
}

const wchar_t *CNicoJK::GetXmlInnerElement(const wchar_t *in, const wchar_t *start_tag, const wchar_t *end_tag, wchar_t *buf, int len) {
	const wchar_t *ps = wcsstr(in, start_tag);
	if (ps) {
		const wchar_t *pe = wcsstr(ps + 1, end_tag);
		if (pe) {
			memset(buf, 0, len * sizeof(wchar_t));
			wcsncpy_s(buf, len, ps + wcslen(start_tag), pe - ps - wcslen(start_tag));
			return pe + wcslen(end_tag);
		}
	}
	return NULL;
}

BOOL CNicoJK::ForceDialog_UpdateForceXML() {
	wchar_t wcs[102400];
	wchar_t channelElem[1024];
	MultiByteToWideChar(CP_UTF8, 0, szChannelBuf_, -1, wcs, 10240);
	const wchar_t *p = wcs;

	TVTest::ChannelInfo info;
	m_pApp->GetCurrentChannelInfo(&info);
	int currentJK = GetJKByChannelName(info.szChannelName);

	HWND hList = GetDlgItem(hForce_, IDC_FORCELIST);
	ListBox_ResetContent(hList);
	while(p = GetXmlInnerElement(p, L"<channel>", L"</channel>", channelElem, 1024)) {
		wchar_t szJK[100];
		GetXmlInnerElement(channelElem, L"<video>", L"</video>", szJK, 100);
		wchar_t szName[100];
		GetXmlInnerElement(channelElem, L"<name>", L"</name>", szName, 100);
		wchar_t szForce[100];
		GetXmlInnerElement(channelElem, L"<force>", L"</force>", szForce, 100);

		wchar_t szResult[1024];
		wsprintfW(szResult, L"%s (%s) 勢い：%s\n", szJK, szName, szForce);
		ListBox_AddString(hList, szResult);

		if (currentJK == _wtoi(szJK + 2)) {
			ListBox_SetCurSel(hList, ListBox_GetCount(hList) - 1);
		}
	}
	return TRUE;
}

BOOL CNicoJK::ForceDialog_OnSelChange(HWND hList) {
	int selected = ListBox_GetCurSel(hList);
	if (selected != LB_ERR) {
		TCHAR sz[1024];
		int len = ListBox_GetTextLen(hList, selected);
		if (3 < len && len < 1024) {
			ListBox_GetText(hList, selected, sz);
			if (_tcsncmp(sz, _T("jk"), 2) == 0) {
				int jkID = _ttoi(sz + 2);
				
				TVTest::ChannelInfo info;
				// なんとなく200まで
				for (int i=0; i<200; i++) {
					if (m_pApp->GetChannelInfo(m_pApp->GetTuningSpace(), i, &info)) {
						int chJK = GetJKByChannelName(info.szChannelName);
						// 実況IDが一致するチャンネルに切替
						if (jkID == chJK) {
							m_pApp->SetChannel(m_pApp->GetTuningSpace(), i);
							return TRUE;
						}
					}
				}
			}
		}
	}
	return FALSE;
}

INT_PTR CALLBACK CNicoJK::ForceDialogProc(HWND hwnd,UINT uMsg,WPARAM wparam,LPARAM lparam) {
	CNicoJK *pThis = (CNicoJK*)GetWindowLongPtr(hwnd, DWL_USER);
	switch(uMsg) {
	case WM_INITDIALOG:
		{
			pThis = (CNicoJK*)lparam;
			SetWindowLongPtr(hwnd, DWL_USER, lparam);

			TCITEM tci;
			tci.mask = TCIF_TEXT;
			tci.pszText = _T("勢い");
			TabCtrl_InsertItem(GetDlgItem(hwnd, IDC_TAB), 0, &tci);

			tci.pszText = _T("ログ");
			TabCtrl_InsertItem(GetDlgItem(hwnd, IDC_TAB), 1, &tci);

			SetTimer(hwnd, TIMER_UPDATE, 20000, NULL);
			// 位置を復元
			int iX = GetPrivateProfileInt(_T("Window"), _T("ForceX"), INT_MAX, pThis->szIniFileName_);
			int iY = GetPrivateProfileInt(_T("Window"), _T("ForceY"), INT_MAX, pThis->szIniFileName_);
			int iW = GetPrivateProfileInt(_T("Window"), _T("ForceWidth"), INT_MAX, pThis->szIniFileName_);
			int iH = GetPrivateProfileInt(_T("Window"), _T("ForceHeight"), INT_MAX, pThis->szIniFileName_);
			HMONITOR hMon = ::MonitorFromWindow(pThis->m_pApp->GetAppWindow(), MONITOR_DEFAULTTONEAREST);
			MONITORINFO mi;
			mi.cbSize = sizeof(MONITORINFO);
			if (::GetMonitorInfo(hMon, &mi)) {
				if (mi.rcMonitor.left <= iX && iX < mi.rcMonitor.right
					&& mi.rcMonitor.top <= iY && iY < mi.rcMonitor.bottom) {
					UINT flag = SWP_NOZORDER;
					if (iW == INT_MAX || iH == INT_MAX) {
						flag |= SWP_NOSIZE;
					}
					SetWindowPos(hwnd, NULL, iX, iY, iW, iH, flag);
				}
			}
		}
		return TRUE;
	case WM_CLOSE:
		{
			RECT rc;
			GetWindowRect(hwnd, &rc);
			TCHAR sz[24];
			wsprintf(sz, _T("%d"), rc.left);
			WritePrivateProfileString(_T("Window"), _T("ForceX"), sz, pThis->szIniFileName_);
			wsprintf(sz, _T("%d"), rc.top);
			WritePrivateProfileString(_T("Window"), _T("ForceY"), sz, pThis->szIniFileName_);
			wsprintf(sz, _T("%d"), rc.right - rc.left);
			WritePrivateProfileString(_T("Window"), _T("ForceWidth"), sz, pThis->szIniFileName_);
			wsprintf(sz, _T("%d"), rc.bottom - rc.top);
			WritePrivateProfileString(_T("Window"), _T("ForceHeight"), sz, pThis->szIniFileName_);
			EndDialog(hwnd, IDOK);
		}
		break;
	
	// jkAPIサーバのホストが得られたら、jkAPIサーバに接続
	case WMS_APIJK_HOST:
		pThis->hGethost_ = NULL;
		if (WSAGETASYNCERROR(lparam) == 0) {
			HOSTENT *hostent = (HOSTENT*)pThis->szHostbuf_;
			pThis->socJkapi_ = socket(PF_INET, SOCK_STREAM, 0);
			if (pThis->socJkapi_ == INVALID_SOCKET) {
				return FALSE;
			}
			WSAAsyncSelect(pThis->socJkapi_, hwnd, WMS_APIJK, FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE);
			
			pThis->serversockaddr_.sin_family = AF_INET;
			pThis->serversockaddr_.sin_addr.s_addr = *(UINT*)(hostent->h_addr);
			pThis->serversockaddr_.sin_port = htons(80);
			memset(pThis->serversockaddr_.sin_zero, 0, sizeof(pThis->serversockaddr_.sin_zero));
			if (connect(pThis->socJkapi_, (struct sockaddr*)&pThis->serversockaddr_, sizeof(pThis->serversockaddr_)) == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					MessageBox(hwnd, _T("ニコニコ実況APIサーバへの接続に失敗しました。"), _T("Error"), MB_OK | MB_ICONERROR);
				}
			}
		}
		return TRUE;

	// jkAPIサーバとの通信
	case WMS_APIJK:
		{
			SOCKET soc = (SOCKET)wparam;
			char *szGetTemplate = "GET /v1/channel.list HTTP/1.0\r\nHost: api.jk.nicovideo.jp\r\nConnection: Close\r\n\r\n";
			switch(WSAGETSELECTEVENT(lparam)) {
			case FD_CONNECT:		/* サーバ接続したイベントの場合 */
				break;
			case FD_WRITE:
				pThis->szChannelBuf_[0] = '\0';
				send(soc, szGetTemplate, strlen(szGetTemplate)+1, 0);
				break;
			case FD_READ:
				char buf[10240];
				int read;
				read = recv(soc, buf, sizeof(buf)-1, 0);
				if (read != SOCKET_ERROR) {
					buf[read] = '\0';
					strcat_s(pThis->szChannelBuf_, buf);
				}
				break;
			case FD_CLOSE:
				if (strlen(buf)) {
					pThis->ForceDialog_UpdateForceXML();
				}
				closesocket(soc);
				pThis->socJkapi_ = INVALID_SOCKET;
				break;
			}
		}
		return TRUE;
	case WM_NEWCOMMENT:
		if (pThis) {
			wchar_t time[15];
			_wstrtime_s(time);
			wchar_t sz[1000];
			wsprintfW(sz, L"%s %s", time, (LPCWSTR)lparam);
			HWND hList = GetDlgItem(hwnd, IDC_LOG);
			int index = ListBox_AddString(hList, sz);
			ListBox_SetTopIndex(hList, index);
			if (index > COMMENT_TRIMSTART) {
				for (; index > COMMENT_TRIMEND; --index) {
					ListBox_DeleteString(hList, 0);
				}
			}
		}
		break;
	case WM_TIMER:
		if (pThis) {
			// 勢いを更新する
			return pThis->ForceDialog_UpdateForce();
		}
		break;
	case WM_SIZE:
		RECT rcParent;
		GetWindowRect(hwnd, &rcParent);
		RECT rcParentClient;
		GetClientRect(hwnd, &rcParentClient);
		RECT rcTab;
		GetClientRect(GetDlgItem(hwnd, IDC_TAB), &rcTab);
		SetWindowPos(GetDlgItem(hwnd, IDC_TAB), NULL, 0, 0, rcParentClient.right, rcTab.bottom, SWP_NOZORDER | SWP_NOMOVE);
		RECT rc;
		GetWindowRect(GetDlgItem(hwnd, IDC_FORCELIST), &rc);
		SetWindowPos(
			GetDlgItem(hwnd, IDC_FORCELIST),
			NULL,
			0,
			0,
			rcParentClient.right - rcParentClient.left,
			rcParentClient.bottom - rcTab.bottom,
			SWP_NOMOVE | SWP_NOZORDER);
		SetWindowPos(
			GetDlgItem(hwnd, IDC_LOG),
			NULL,
			0,
			0,
			rcParentClient.right - rcParentClient.left,
			rcParentClient.bottom - rcTab.bottom,
			SWP_NOMOVE | SWP_NOZORDER);
		break;
	case WM_COMMAND:
		if (HIWORD(wparam) == LBN_SELCHANGE) {
			if (LOWORD(wparam) == IDC_FORCELIST && pThis) {
				// リストで選択したチャンネルに切り替える
				return pThis->ForceDialog_OnSelChange((HWND)lparam);
			}
		}
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR)lparam)->code) {
		case TCN_SELCHANGE:
			if (((LPNMHDR)lparam)->idFrom == IDC_TAB) {
				HWND hTab = ((LPNMHDR)lparam)->hwndFrom;
				int selected = TabCtrl_GetCurSel(hTab);
				switch (selected) {
				case 0:
					ShowWindow(GetDlgItem(hwnd, IDC_FORCELIST), SW_SHOW);
					ShowWindow(GetDlgItem(hwnd, IDC_LOG), SW_HIDE);
					break;
				case 1:
					ShowWindow(GetDlgItem(hwnd, IDC_FORCELIST), SW_HIDE);
					ShowWindow(GetDlgItem(hwnd, IDC_LOG), SW_SHOW);
					break;
				}
			}
		}
		break;
	default:
		break;
	}
	return FALSE;
}

TVTest::CTVTestPlugin *CreatePluginClass()
{
	return new CNicoJK();
}
