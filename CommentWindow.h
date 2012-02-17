
#pragma once

#include <Windows.h>
#include <tchar.h>
#include "TVTestPlugin.h"

#define jkTimerID 1021
#define IDB_START 1

class Cjk {
	TVTest::CTVTestApp *m_pApp;

	HWND hWnd_;
	// Draw
	HDC memDC_;
	HBITMAP hBitmap_;
	HBITMAP hPrevBitmap_;

	int msPosition_;
	int msSystime_;
	int msPositionBase_;

	// 通信用
	int jkCh_;
	char szHostbuf_[MAXGETHOSTSTRUCT];
	char szThread_[1024];
	SOCKET socGetflv_;
	SOCKET socComment_;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

public:
	Cjk(TVTest::CTVTestApp *pApp);
	void Create(HWND hParent);
	void Destroy();
	void Resize(int left, int top, int width, int height);
	void Resize(HWND hApp);
	HWND GetFullscreenWindow();

	void Open(int jkCh);
	void Open(LPCTSTR filename);
	void SetPosition(int posms);
	void Start();

	// Draw
	void SetupObjects();
	void ClearObjects();
	void DrawComments(HWND hWnd, HDC hDC);

	// TVTestのメッセージ
	BOOL WindowMsgCallback(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam,LRESULT *pResult);
	LRESULT EventCallback(UINT Event, LPARAM lParam1, LPARAM lParam2);
};
