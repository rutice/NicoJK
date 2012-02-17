// NicoJK.h
#pragma once

#include "stdafx.h"

#define TVTEST_PLUGIN_CLASS_IMPLEMENT
#include "TVTestPlugin.h"
#include "CommentWindow.h"

// ライブラリ
#pragma comment(lib, "shlwapi.lib")

// ニコニコ実況のCOM DLL（パスが違う場合は修正する）
#import "C:\Program Files (x86)\niwango\nicom\jkNiCOM.dll"  no_namespace named_guids raw_interfaces_only

// 定数
const UINT_PTR TIMER_UPDATE = 1;

// プラグインクラス
class CNicoJK : public TVTest::CTVTestPlugin
{
	// ニコニコ実況SDK
	IJKNiCOMPtr jk_;
	IChannelCollectionPtr channels_;
	ICommentWindowPtr cw_;
	IChannelPtr channel_;

	// 実況表示中
	bool isJK;
	// 設定ファイルの名前 (tvtpをiniに変えたもの）
	TCHAR szIniFileName_[2048];
	// チャンネル変更時のウェイトタイマ
	UINT_PTR timerID_;
	static CNicoJK *this_; // static用
	// 重ねるモード
	int cwMode_;

	// 勢い窓
	HWND hForce_;

	// 独自実装用
	bool useSDK_;
	Cjk *jkcw_;

	static VOID CALLBACK OnServiceChangeTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
	static LRESULT CALLBACK EventCallback(UINT Event,LPARAM lParam1,LPARAM lParam2,void *pClientData);
	static BOOL CALLBACK WindowMsgCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *pResult, void *pUserData);

	static INT_PTR CALLBACK ForceDialogProc(HWND hwnd,UINT uMsg,WPARAM wparam,LPARAM lparam);
	BOOL ForceDialog_UpdateForce(HWND hWnd);
	BOOL ForceDialog_OnSelChange(HWND hList);

	void TogglePlugin(bool bEnabled);

	void OnChannelChange();
	void OnFullScreenChange();

	void StartJK(int jkID);
	void StopJK();
	void AdjustCommentWindow();

	HWND GetFullscreenWindow();
public:
	virtual bool GetPluginInfo(TVTest::PluginInfo *pInfo);
	virtual bool Initialize();
	virtual bool Finalize();
};
