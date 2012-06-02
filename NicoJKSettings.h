#pragma once
#include <string>
#include "stdafx.h"

/* ニコニコ実況基本設定 */
struct TNicoJKSettings
{
	/* 行間倍率 */
	int				timerInterval;
	float			commentLineMargin;
	float			commentFontOutline;
	float			commentSize;
	int				commentSizeMin;
	int				commentSizeMax;
	std::wstring	commentFontName;
	int				commentFontBold;
};

class CNJIni
{
	static TNicoJKSettings		settings;

public:
	static void LoadFromIni(TCHAR* iniFile);
	static TNicoJKSettings* GetSettings() { return &settings; }
};
