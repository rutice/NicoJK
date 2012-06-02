#include "stdafx.h"
#include "NicoJKSettings.h"

TNicoJKSettings		CNJIni::settings;

void CNJIni::LoadFromIni(TCHAR* iniFile)
{
	TCHAR	str[1024];
	memset(&settings, 0, sizeof(settings));
	settings.timerInterval = GetPrivateProfileInt(_T("Setting"), _T("timerInterval"), 33, iniFile);
	settings.commentLineMargin = static_cast<float>(GetPrivateProfileInt(_T("Setting"), _T("commentLineMargin"), 125, iniFile)) / 100.0f;
	settings.commentFontOutline = static_cast<float>(GetPrivateProfileInt(_T("Setting"), _T("commentFontOutline"), 300, iniFile)) / 100.0f;
	settings.commentSize = static_cast<float>(GetPrivateProfileInt(_T("Setting"), _T("commentSize"), 100, iniFile)) / 100.0f;
	settings.commentSizeMin = GetPrivateProfileInt(_T("Setting"), _T("commentSizeMin"), 16, iniFile);
	settings.commentSizeMax = GetPrivateProfileInt(_T("Setting"), _T("commentSizeMax"), 9999, iniFile);
	GetPrivateProfileString(_T("Setting"), _T("commentFontName"), _T("‚l‚r ‚oƒSƒVƒbƒN"), str, 1024, iniFile);
	settings.commentFontName.assign(str);
	settings.commentFontBold = GetPrivateProfileInt(_T("Setting"), _T("commentFontBold"), 125, iniFile);
}
