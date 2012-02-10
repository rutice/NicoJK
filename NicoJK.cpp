// NicoJK.cpp
//
/*
	NicoJK
		TVTest ニコニコ実況プラグイン
*/

#include "stdafx.h"
#include "NicoJK.h"

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

	CoInitialize(NULL);

	// イベントコールバック関数を登録
	m_pApp->SetEventCallback(EventCallback, this);
	// ウィンドウコールバック関数を登録
	m_pApp->SetWindowMessageCallback(WindowMsgCallback, this);

	if (m_pApp->IsPluginEnabled()) {
		TVTest::ChannelInfo info;
		m_pApp->GetCurrentChannelInfo(&info);
		StartJK(info.RemoteControlKeyID);
	}

	return true;
}

void CNicoJK::StartJK(int jkID) {
	StopJK();
	isJK = true;

	HRESULT hr;
	jk_.CreateInstance(CLSID_JKNiCOM);
	if (!jk_) {
		MessageBox(NULL, _T("ニコニコ実況SDKが入ってないかも。"), NULL, MB_OK);
		isJK = false;
		return ;
	}

	long val;
	jk_->GetCurrentVersion(&val);
	if (val < 110) {
		MessageBox(NULL, _T("ニコニコ実況SDKが古いかも。"), NULL, MB_OK);
		jk_.Release();
		isJK = false;
		return ;
	}

	jk_->get_Channels(&channels_);
	if (!channels_) {
		MessageBox(NULL, _T("チャンネル一覧がとれませんでした。"), NULL, MB_OK);
		jk_.Release();
		isJK = false;
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
		target = GetNormalHWND();
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
		if (!target) {
			target = this->GetNormalHWND();
			if (target) {
				target = FindWindowEx(m_pApp->GetAppWindow(), NULL, _T("TVTest Splitter"), NULL);
				if (target) {
					target = FindWindowEx(target, NULL, _T("TVTest View"), NULL);
					if (target) {
						target = FindWindowEx(target, NULL, _T("TVTest Video Container"), NULL);
					}
				}
			}
		}
		RECT rc;
		GetWindowRect(target, &rc);
		cw_->put_X(rc.left);
		cw_->put_Y(rc.top);
		cw_->put_Width(rc.right - rc.left);
		cw_->put_Height(rc.bottom - rc.top);
	}
}

HWND CNicoJK::GetFullscreenWindow() {
	TVTest::HostInfo hostInfo;
	if (m_pApp->GetFullscreen() && m_pApp->GetHostInfo(&hostInfo)) {
		wchar_t className[64];
		wcsncpy_s(className, hostInfo.pszAppName, 48);
		wcscat_s(className, L" Fullscreen");

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

HWND CNicoJK::GetNormalHWND() {
	return m_pApp->GetAppWindow();
}

void CNicoJK::OnChannelChange() {
	TVTest::ChannelInfo info;
	m_pApp->GetCurrentChannelInfo(&info);
	OutputDebugStringW(info.szChannelName);

	int jkID = GetPrivateProfileInt(_T("Setting"), info.szChannelName, -1, szIniFileName_);
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
			cw_->AttachWindowByHandle(HandleToLong(GetNormalHWND()), &hr);
		}
	}
}

bool CNicoJK::Finalize() {
	// 終了処理	
	StopJK();

	CoUninitialize();
	return true;
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

	switch (Event) {
	case TVTest::EVENT_PLUGINENABLE:
		if (lParam1 != 0) {
			pThis->OnChannelChange();
		} else {
			pThis->StopJK();
		}
		return TRUE;
	case TVTest::EVENT_FULLSCREENCHANGE:
		if (pThis->m_pApp->IsPluginEnabled()) {
			pThis->OnFullScreenChange();
		}
		return TRUE;
	case TVTest::EVENT_CHANNELCHANGE:
	case TVTest::EVENT_SERVICECHANGE:
		if (pThis->timerID_) {
			KillTimer(NULL, pThis->timerID_);
		}
		pThis->timerID_ = SetTimer(NULL, 0, 750, pThis->OnServiceChangeTimer);
		return TRUE;
	}
	return 0;
}

BOOL CALLBACK CNicoJK::WindowMsgCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *pResult, void *pUserData)
{
	CNicoJK *pThis = static_cast<CNicoJK*>(pUserData);

	switch (uMsg) {
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			if (pThis->m_pApp->IsPluginEnabled()) {
				if (pThis->m_pApp->GetFullscreen()) {
					pThis->AdjustCommentWindow();
					return TRUE;
				}
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

TVTest::CTVTestPlugin *CreatePluginClass()
{
	return new CNicoJK();
}
