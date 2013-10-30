#include "stdafx.h"
#include "AsyncSocket.h"

CAsyncSocket::CAsyncSocket()
	: hGethost_(NULL)
	, soc_(INVALID_SOCKET)
	, bReady_(false)
	, bShutdown_(false)
	, hwnd_(NULL)
	, msg_(0)
	, name_(NULL)
	, port_(0)
	, bKeepSession_(false)
	, bDoHalfClose_(false)
{
}

CAsyncSocket::~CAsyncSocket()
{
	Close();
	delete [] name_;
}

// 非同期通信を開始する
// すでに開始しているときは失敗するが、name==NULLのときは開いているソケットに送信データを追加する
bool CAsyncSocket::Send(HWND hwnd, UINT msg, const char *name, unsigned short port, const char *buf, int len, bool bKeepSession)
{
	if (len < 0) {
		len = lstrlenA(buf);
	}
	if (len > 0) {
		// 前のデータを送信済みのときだけ送信データを追加できる
		if (IsPending() && !name && bKeepSession_ && sendBuf_.empty()) {
			sendBuf_.assign(&buf[0], &buf[len]);
			bKeepSession_ = bKeepSession;
			PostMessage(hwnd_, msg_, (WPARAM)soc_, WSAMAKESELECTREPLY(FD_WRITE, 0));
			return true;
		} else if (!IsPending() && name) {
			sendBuf_.assign(&buf[0], &buf[len]);
			hwnd_ = hwnd;
			msg_ = msg;
			delete [] name_;
			name_ = new char[lstrlenA(name) + 1];
			lstrcpyA(name_, name);
			port_ = port;
			bKeepSession_ = bKeepSession;
			// キューにたまっているかもしれないメッセージを流すため待機
			bReady_ = true;
			PostMessage(hwnd, msg, 0, 0);
			return true;
		}
	}
	return false;
}

// ウィンドウメッセージを処理してデータを受信する
// 受信データはrecvBufに追記される
// 戻り値: 負値=切断した(-2=正常,-1=中断), 0=正常に処理した
int CAsyncSocket::ProcessRecv(WPARAM wParam, LPARAM lParam, std::vector<char> *recvBuf)
{
	UINT imAddr = INADDR_NONE;

	if (bReady_) {
		// 待機中
		if (wParam || lParam) {
			return 0;
		}
		bReady_ = false;
		if (bShutdown_) {
			bShutdown_ = false;
			return -1;
		}
		if ((imAddr = inet_addr(name_)) == INADDR_NONE) {
			hGethost_ = WSAAsyncGetHostByName(hwnd_, msg_, name_, hostBuf_, sizeof(hostBuf_));
			return hGethost_ ? 0 : -1;
		}
		// IPアドレス即値(名前解決を省略)

	} else if (hGethost_) {
		// 名前解決中
		bool bValid = wParam == (WPARAM)hGethost_ && WSAGETASYNCERROR(lParam) == 0;
		hGethost_ = NULL;
		if (bShutdown_) {
			bShutdown_ = false;
			return -1;
		}
		if (!bValid || (imAddr = *(UINT*)((HOSTENT*)hostBuf_)->h_addr) == INADDR_NONE) {
			return -1;
		}
	}

	if (imAddr != INADDR_NONE) {
		// 接続
		if ((soc_ = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
			return -1;
		}
		WSAAsyncSelect(soc_, hwnd_, msg_, FD_WRITE | FD_READ | FD_CLOSE);
		struct sockaddr_in addr = {0};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = imAddr;
		addr.sin_port = htons(port_);
		if (connect(soc_, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
			closesocket(soc_);
			soc_ = INVALID_SOCKET;
			return -1;
		}
		return 0;
	}

	if (soc_ != INVALID_SOCKET) {
		// 送受信中
		if (wParam == (WPARAM)soc_) {
			switch(WSAGETSELECTEVENT(lParam)) {
			case FD_WRITE:
				if (!bShutdown_) {
					while (!sendBuf_.empty()) {
						int wrote = send(soc_, &sendBuf_.front(), (int)sendBuf_.size(), 0);
						if (wrote == SOCKET_ERROR) {
							// WSAEWOULDBLOCK時はつぎのFD_WRITEに先送り
							if (WSAGetLastError() != WSAEWOULDBLOCK) {
								Shutdown();
							}
							break;
						}
						sendBuf_.erase(sendBuf_.begin(), sendBuf_.begin() + wrote);
					}
					if (sendBuf_.empty() && !bKeepSession_ && bDoHalfClose_) {
						// ハーフクローズ
						// Bitdefender環境で受信が中断する不具合を確認(2013-10-28)
						shutdown(soc_, SD_SEND);
					}
				}
				return 0;
			case FD_READ:
				for (;;) {
					char buf[2048];
					int read = recv(soc_, buf, sizeof(buf), 0);
					if (read == SOCKET_ERROR || read <= 0) {
						// FD_CLOSEで拾うので無視
						return 0;
					}
					recvBuf->insert(recvBuf->end(), &buf[0], &buf[read]);
				}
				break;
			case FD_CLOSE:
				if (WSAGETSELECTERROR(lParam) != 0) {
					Close();
					return -1;
				}
				for (;;) {
					char buf[2048];
					int read = recv(soc_, buf, sizeof(buf), 0);
					if (read == SOCKET_ERROR || read <= 0) {
						Close();
						return read == 0 ? -2 : -1;
					}
					recvBuf->insert(recvBuf->end(), &buf[0], &buf[read]);
				}
				break;
			}
		}
		return 0;
	}
	return -1;
}

// 送受信停止を要求する
// 呼び出し後ProcessRecv()が負値を返すと完了(ソケットも閉じられる)
bool CAsyncSocket::Shutdown()
{
	if (IsPending() && !bShutdown_) {
		if (soc_ != INVALID_SOCKET) {
			shutdown(soc_, SD_BOTH);
		}
		bShutdown_ = true;
	}
	return bShutdown_;
}

// ソケットを強制的に閉じる
// ポストされたメッセージが残る可能性があるのでなるべくShutdown()を使う
void CAsyncSocket::Close()
{
	if (hGethost_) {
		WSACancelAsyncRequest(hGethost_);
		hGethost_ = NULL;
	}
	if (soc_ != INVALID_SOCKET) {
		closesocket(soc_);
		soc_ = INVALID_SOCKET;
	}
	bReady_ = false;
	bShutdown_ = false;
}
