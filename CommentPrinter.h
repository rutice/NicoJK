
#pragma once

class Chat;

interface Printer {
	virtual ~Printer(){ };
	virtual const TCHAR *GetPrinterName() = 0;
	virtual void SetParams(HWND hWnd, int fontSize) = 0;
	virtual SIZE GetTextSize(const wchar_t *text, int len) = 0;
	virtual void Begin(RECT rcWnd, int yPitch) = 0;
	virtual void End() = 0;
	virtual void DrawShita(const Chat &chat) = 0;
	virtual void DrawNormal(const Chat &chat, int vpos) = 0;
};

Printer *CreatePrinter(bool disableDW);
