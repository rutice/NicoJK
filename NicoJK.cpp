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

	// 独自実装？
	useSDK_ = 0 != GetPrivateProfileInt(_T("Setting"), _T("useSDK"), 1, szIniFileName_);
	jkcw_ = new Cjk(m_pApp);

	CoInitialize(NULL);
	WSADATA wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);

	// 勢い窓作成
	hForce_ = CreateDialogParam(g_hinstDLL, MAKEINTRESOURCE(IDD_FORCE),
									 NULL, (DLGPROC)ForceDialogProc, (LPARAM)this);

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

void CNicoJK::OnChannelChange() {
	TVTest::ChannelInfo info;
	m_pApp->GetCurrentChannelInfo(&info);
	OutputDebugStringW(info.szChannelName);

	int jkID = GetPrivateProfileInt(_T("Channels"), info.szChannelName, -1, szIniFileName_);
	if (jkID == -1) {
		jkID = GetPrivateProfileInt(_T("Setting"), info.szChannelName, -1, szIniFileName_);
	}

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
BOOL CNicoJK::ForceDialog_UpdateForce(HWND hWnd) {
	HWND hList = GetDlgItem(hWnd, IDC_FORCELIST);
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
						int chJK = GetPrivateProfileIntW(_T("Channels"), info.szChannelName, -1, szIniFileName_);
						if (chJK == -1) {
							chJK = GetPrivateProfileIntW(_T("Setting"), info.szChannelName, -1, szIniFileName_);
						}
						// 実況IDが一致するチャンネルに切替
						if (jkID == chJK) {
							m_pApp->GetCurrentChannelInfo(&info);
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
			SetTimer(hwnd, TIMER_UPDATE, 20000, NULL);
			// 位置を復元
			int iX = GetPrivateProfileInt(_T("Window"), _T("ForceX"), INT_MAX, pThis->szIniFileName_);
			int iY = GetPrivateProfileInt(_T("Window"), _T("ForceY"), INT_MAX, pThis->szIniFileName_);
			HMONITOR hMon = ::MonitorFromWindow(pThis->m_pApp->GetAppWindow(), MONITOR_DEFAULTTONEAREST);
			MONITORINFO mi;
			mi.cbSize = sizeof(MONITORINFO);
			if (::GetMonitorInfo(hMon, &mi)) {
				if (mi.rcMonitor.left <= iX && iX < mi.rcMonitor.right
					&& mi.rcMonitor.top <= iY && iY < mi.rcMonitor.bottom) {
					SetWindowPos(hwnd, NULL, iX, iY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
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
			EndDialog(hwnd, IDOK);
		}
		break;
	case WM_TIMER:
		if (pThis) {
			// 勢いを更新する
			return pThis->ForceDialog_UpdateForce(hwnd);
		}
		break;
	case WM_COMMAND:
		if (HIWORD(wparam) == LBN_SELCHANGE) {
			if (pThis) {
				// リストで選択したチャンネルに切り替える
				return pThis->ForceDialog_OnSelChange((HWND)lparam);
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
