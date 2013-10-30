#pragma once

// おもにGETリクエストを想定した非同期TCPクライアント
class CAsyncSocket
{
public:
	CAsyncSocket();
	~CAsyncSocket();
	bool Send(HWND hwnd, UINT msg, const char *name, unsigned short port, const char *buf, int len = -1, bool bKeepSession = false);
	int ProcessRecv(WPARAM wParam, LPARAM lParam, std::vector<char> *recvBuf);
	bool Shutdown();
	void Close();
	bool IsPending() const { return bReady_ || hGethost_ || soc_ != INVALID_SOCKET; }
	void SetDoHalfClose(bool bDoHalfClose) { bDoHalfClose_ = bDoHalfClose; }
private:
	HANDLE hGethost_;
	SOCKET soc_;
	bool bReady_;
	bool bShutdown_;
	HWND hwnd_;
	UINT msg_;
	char *name_;
	unsigned short port_;
	bool bKeepSession_;
	bool bDoHalfClose_;
	std::vector<char> sendBuf_;
	char hostBuf_[MAXGETHOSTSTRUCT];
};
