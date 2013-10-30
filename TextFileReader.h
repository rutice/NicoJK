#pragma once

// マルチバイトテキストファイル読み込み(ReadFileのラッパ)
class CTextFileReader
{
public:
	static const int BUF_SIZE = 512;
	CTextFileReader();
	~CTextFileReader();
	bool Open(LPCTSTR path, DWORD shareMode, DWORD flagsAndAttributes);
	void Close();
	bool ResetPointer();
	int ReadLine(char *text, int textMax);
	int ReadLastLine(char *text, int textMax);
	int Seek(int scale);
	bool IsOpen() const { return hFile_ != INVALID_HANDLE_VALUE; }
private:
	HANDLE hFile_;
	bool bEof_;
	char buf_[BUF_SIZE];
};
