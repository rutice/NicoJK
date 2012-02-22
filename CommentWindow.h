
#pragma once

#include "stdafx.h"
#include "TVTestPlugin.h"

#include <string>
#include <vector>
#include <list>
#include <cstdio>
#include <algorithm>
#include <functional>

#define jkTimerID 1021
#define IDB_START 1

#define COLOR_TRANSPARENT RGB(12, 12, 12)

#define WM_NEWCOMMENT (WM_APP+200)

class Cjk {
	TVTest::CTVTestApp *m_pApp;

	HWND hWnd_;
	HWND hForce_;
	// Draw
	HDC memDC_;
	HBITMAP hBitmap_;
	HBITMAP hPrevBitmap_;

	DWORD msPosition_;
	DWORD msSystime_;
	DWORD msPositionBase_;

	// 通信用
	int jkCh_;
	char szHostbuf_[MAXGETHOSTSTRUCT];
	char szThread_[1024];
	SOCKET socGetflv_;
	SOCKET socComment_;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

public:
	Cjk(TVTest::CTVTestApp *pApp, HWND hForce);
	void Create(HWND hParent);
	void Destroy();
	void Resize(int left, int top, int width, int height);
	void Resize(HWND hApp);
	HWND GetFullscreenWindow();

	void Open(int jkCh);
	void SetPosition(int posms);
	void SetLiveMode();
	void Start();

	// Draw
	void SetupObjects();
	void ClearObjects();
	void DrawComments(HWND hWnd, HDC hDC);

	// TVTestのメッセージ
	BOOL WindowMsgCallback(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam,LRESULT *pResult);
	LRESULT EventCallback(UINT Event, LPARAM lParam1, LPARAM lParam2);
};
