
#include "stdafx.h"
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <intsafe.h>
#include <d3d10.h>

//#include <wincodec.h>
//#include <wincodecsdk.h>
#include <comip.h>

#include "CommentPrinter.h"
#include "CommentWindow.h"
#include "Direct2DRenderer.h"
#include "NicoJKSettings.h"

//#pragma comment(lib, "WindowsCodecs.lib")

HFONT CreateFont(int h) {
	return ::CreateFont(h,    //フォント高さ
		0,                    //文字幅
		0,                    //テキストの角度
		0,                    //ベースラインとｘ軸との角度
		CNJIni::GetSettings()->commentFontBold ? FW_BOLD : FW_NORMAL,            //フォントの重さ（太さ）
		FALSE,                //イタリック体
		FALSE,                //アンダーライン
		FALSE,                //打ち消し線
		SHIFTJIS_CHARSET,    //文字セット
		OUT_DEFAULT_PRECIS,    //出力精度
		CLIP_DEFAULT_PRECIS,//クリッピング精度
		NONANTIALIASED_QUALITY,        //出力品質
		FIXED_PITCH | FF_MODERN,//ピッチとファミリー
		CNJIni::GetSettings()->commentFontName.c_str());    //書体名
}

class GDIPrinter : public Printer {
	HWND hWnd_;
	HDC hDC_;
	HFONT hFont_;
	HGDIOBJ hOld_;

	HDC memDC_;
	HBITMAP hBitmap_;
	HBITMAP hPrevBitmap_;
public:
	GDIPrinter() :
	  hWnd_(NULL),
		  hDC_(NULL)
	  {
		  HDC wndDC = GetDC(hWnd_);
		  memDC_ = CreateCompatibleDC(wndDC);
		  hBitmap_ = CreateCompatibleBitmap(wndDC, 1920, 1200);
		  hPrevBitmap_ = (HBITMAP)SelectObject(memDC_, hBitmap_);

		  ReleaseDC(hWnd_, wndDC);
	  }

	  ~GDIPrinter() {
		  if (memDC_) {
			  SelectObject(memDC_, hPrevBitmap_);
			  DeleteObject(hBitmap_);
			  DeleteDC(memDC_);
		  }
		  if (hDC_) {	
			  SelectObject(hDC_, hOld_);
			  DeleteObject(hFont_);
			  ReleaseDC(hWnd_, hDC_);
		  }
	  }

	  const TCHAR *GetPrinterName() {
		  return _T("GDI");
	  }

	  SIZE GetTextSize(const wchar_t *text, int len) {
		  SIZE size;
		  if (GetTextExtentPoint32W(memDC_, text, len, &size)) {
			  return size;
		  }
		  return SIZE();
	  }

	  void SetParams(HWND hWnd, int fontSize) {
		  if (hDC_) {
			  SelectObject(hDC_, hOld_);
			  DeleteObject(hFont_);
			  ReleaseDC(hWnd_, hDC_);
		  }
		  hWnd_ = hWnd;
		  hDC_ = GetDC(hWnd);
		  hFont_ = CreateFont(fontSize);
		  hOld_ = SelectObject(memDC_, hFont_);
		  SetLayeredWindowAttributes(hWnd, COLOR_TRANSPARENT, 0, LWA_COLORKEY);
	  }

	  RECT rcWnd_;
	  int yPitch_;
	  void Begin(RECT rcWnd, int yPitch) {
		  CopyRect(&rcWnd_, &rcWnd);
		  yPitch_ = yPitch;
		  HBRUSH transBrush = CreateSolidBrush(COLOR_TRANSPARENT);
		  FillRect(memDC_, &rcWnd, transBrush);
		  DeleteObject(transBrush);

		  int width = rcWnd.right;

		  SetBkMode(memDC_, TRANSPARENT);
	  }

	  void End() {
		  RECT rcWnd;
		  GetClientRect(hWnd_, &rcWnd);
		  BitBlt(hDC_, 0, 0, rcWnd.right - rcWnd.left, rcWnd.bottom - rcWnd.top, memDC_, 0, 0, SRCCOPY);
	  }

	  void DrawShita(const Chat &chat) {
		  RECT rc;
		  rc.top = rcWnd_.bottom - yPitch_ - static_cast<LONG>(chat.line * (int)((float)yPitch_ * CNJIni::GetSettings()->commentLineMargin));
		  rc.bottom = rc.top + yPitch_;
		  rc.left = 0;
		  rc.right = rcWnd_.right;
		  int shadow = (yPitch_ / 16);
		  if ( yPitch_ > 24 && shadow == 1 ) { shadow = 2; }
		  if ( shadow < 1 ) { shadow = 1; }
		  const std::wstring &text = chat.text;
		  SetTextColor(memDC_, RGB(0, 0, 0));
		  DrawTextW(memDC_, text.c_str(), text.length(), &rc, DT_CENTER | DT_NOPREFIX);
		  OffsetRect(&rc, -shadow, -shadow);
		  SetTextColor(memDC_, chat.color);
		  DrawTextW(memDC_, text.c_str(), text.length(), &rc, DT_CENTER | DT_NOPREFIX);
	  }

	  void DrawNormal(const Chat &chat, int vpos) {
		  int x = static_cast<int>(rcWnd_.right - (float)(chat.width + rcWnd_.right) * (vpos - chat.vpos) / VPOS_LEN);
		  int y = static_cast<int>(((float)yPitch_ * CNJIni::GetSettings()->commentLineMargin) * (float)chat.line);
		  int shadow = (yPitch_ / 16);
		  if ( yPitch_ > 24 && shadow == 1 ) { shadow = 2; }
		  if ( shadow < 1 ) { shadow = 1; }
		  const std::wstring &text = chat.text;
		  SetTextColor(memDC_, RGB(0, 0, 0));
		  TextOutW(memDC_, x+shadow, y+shadow, text.c_str(), text.length());
		  SetTextColor(memDC_, chat.color);
		  TextOutW(memDC_, x, y, text.c_str(), text.length());
	  }
};

#define COMPTR_TYPEDEF(Interface) typedef _com_ptr_t<_com_IIID<Interface, &__uuidof(Interface)> > Interface ## Ptr
class DWPrinter : public Printer {
	HWND hWnd_;

	// how much to scale a design that assumes 96-DPI pixels
	float dpiScaleX_;
	float dpiScaleY_;

	// Direct2D
	COMPTR_TYPEDEF(ID2D1Factory);
	ID2D1FactoryPtr pD2DFactory_;

	COMPTR_TYPEDEF(ID2D1RenderTarget);
	COMPTR_TYPEDEF(ID2D1GdiInteropRenderTarget);
	ID2D1RenderTargetPtr pRT_;
	ID2D1GdiInteropRenderTargetPtr pGDITarget_;

	COMPTR_TYPEDEF(ID2D1SolidColorBrush);
	ID2D1SolidColorBrushPtr pBlackBrush_;

	// WIC
	//COMPTR_TYPEDEF(IWICBitmap);
	//IWICBitmapPtr bitmap_;

	// DirectWrite
	COMPTR_TYPEDEF(IDWriteFactory);
	IDWriteFactoryPtr pDWriteFactory_;
	COMPTR_TYPEDEF(IDWriteTextFormat);
	IDWriteTextFormatPtr pTextFormat_;

	// CustomRenderer
	CustomTextRenderer *renderer_;
	COMPTR_TYPEDEF(IDWriteTextRenderer);
	IDWriteTextRendererPtr pTextRenderer_;

	// font
	int iFontSize_;
	HFONT hFont_;

	// API
	typedef HRESULT (WINAPI *DWriteCreateFactoryPtr)(
		__in DWRITE_FACTORY_TYPE factoryType,
		__in REFIID iid,
		__out IUnknown **factory
		);

	typedef HRESULT (WINAPI *D2D1CreateFactoryPtr)(
		__in D2D1_FACTORY_TYPE factoryType,
		__in REFIID riid,
		__in_opt CONST D2D1_FACTORY_OPTIONS *pFactoryOptions,
		__out void **ppIFactory
		);

	typedef HRESULT (WINAPI* PFN_D3D10_CREATE_DEVICE1)(IDXGIAdapter *, 
    D3D10_DRIVER_TYPE, HMODULE, UINT, D3D10_FEATURE_LEVEL1, UINT, ID3D10Device1**);

	static D2D1CreateFactoryPtr pD2D1CreateFactory;
	static DWriteCreateFactoryPtr pDWriteCreateFactory;
	static PFN_D3D10_CREATE_DEVICE1 pD3D10CreateDevice1;
	static int isAvailable; // -1: unchecked, 0: no, 1:yes

	UINT32 bgr2rgb(UINT32 bgr) {
		return ((bgr & 0xFF0000) >> 16) + (bgr & 0xFF00) + ((bgr & 0xFF) << 16);
	}

public:
	static bool IsAvailableImpl() {
		if (pD2D1CreateFactory && pDWriteCreateFactory && pD3D10CreateDevice1) {
			return true;
		}

		TCHAR dllPath[1024];
		GetSystemDirectory(dllPath, 1024);
		if (dllPath[_tcslen(dllPath)-1] != _T('\\')) {
			if (_tcscat_s(dllPath, 1024, _T("\\")) != 0) {
				return false;
			}
		}

		if (_tcscat_s(dllPath, 1024, _T("d2d1.dll")) != 0) {
			return false;
		}
		HMODULE hD2 = LoadLibrary(dllPath);
		if (!hD2) {
			return false;
		}
		pD2D1CreateFactory = (D2D1CreateFactoryPtr)GetProcAddress(hD2, "D2D1CreateFactory");
		if (!pD2D1CreateFactory) {
			return false;
		}

		GetSystemDirectory(dllPath, 1024);
		if (dllPath[_tcslen(dllPath)-1] != _T('\\')) {
			if (_tcscat_s(dllPath, 1024, _T("\\")) != 0) {
				return false;
			}
		}
		if (_tcscat_s(dllPath, 1024, _T("DWrite.dll")) != 0) {
			return false;
		}
		HMODULE hDW = LoadLibrary(dllPath);
		if (!hDW) {
			FreeLibrary(hD2);
			return false;
		}
		pDWriteCreateFactory = (DWriteCreateFactoryPtr)GetProcAddress(hDW, "DWriteCreateFactory");
		if (!pDWriteCreateFactory) {
			FreeLibrary(hD2);
			FreeLibrary(hDW);
			return false;
		}

		GetSystemDirectory(dllPath, 1024);
		if (dllPath[_tcslen(dllPath)-1] != _T('\\')) {
			if (_tcscat_s(dllPath, 1024, _T("\\")) != 0) {
				return false;
			}
		}
		if (_tcscat_s(dllPath, 1024, _T("d3d10_1.dll")) != 0) {
			return false;
		}
		HMODULE hD3D = LoadLibrary(dllPath);
		if (!hD3D) {
			FreeLibrary(hDW);
			FreeLibrary(hD2);
			return false;
		}
		pD3D10CreateDevice1 = (PFN_D3D10_CREATE_DEVICE1)GetProcAddress(hD3D, "D3D10CreateDevice1");
		if (!pD3D10CreateDevice1) {
			FreeLibrary(hD3D);
			FreeLibrary(hD2);
			FreeLibrary(hDW);
			return false;
		}

		// HARDWAREレンダリングが使えるかテスト
		DWPrinter *pThis = new DWPrinter();
		HRESULT hr = pThis->CreateDeviceIndependentResources();
		if (SUCCEEDED(hr)) {
			hr = pThis->CreateDeviceResources();
		}
		if (FAILED(hr) || !pThis->pRT_) {
			delete pThis;
			FreeLibrary(hD2);
			FreeLibrary(hDW);
			return false;
		}

		return true;
	}

	static bool IsAvailable() {
		if (isAvailable == -1) {
			isAvailable = IsAvailableImpl() ? 1 : 0;
		}
		return isAvailable != 0;
	}

	DWPrinter() :
	hWnd_(NULL),
		iFontSize_(12),
		hFont_(NULL) {

			if (!IsAvailable()) {
				return;
			}
			HDC screen = GetDC(0);
			dpiScaleX_ = GetDeviceCaps(screen, LOGPIXELSX) / 96.0f;
			dpiScaleY_ = GetDeviceCaps(screen, LOGPIXELSY) / 96.0f;
			ReleaseDC(0, screen);
			/*
			typedef _com_ptr_t<_com_IIID<IWICImagingFactory, &__uuidof(IWICImagingFactory)>> IWICImagingFactoryPtr;
			IWICImagingFactoryPtr factory;
			factory.CreateInstance(CLSID_WICImagingFactory);

			factory->CreateBitmap(
				1920,
				1080,
				GUID_WICPixelFormat32bppPBGRA,
				WICBitmapCacheOnLoad,
				&bitmap_);*/
	}

	~DWPrinter() {
	}

	const TCHAR *GetPrinterName() {
		return _T("DirectWrite");
	}

	void SetParams(HWND hWnd, int fontSize) {
		hWnd_ = hWnd;
		iFontSize_ = fontSize;
		CreateDeviceIndependentResources();

		if (hFont_) {
			DeleteObject(hFont_);
		}
		hFont_ = CreateFont(fontSize);
	}

	HRESULT CreateDeviceIndependentResources() {
		HRESULT hr;

		hr = pD2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory),
			NULL,
			(void**)&pD2DFactory_
			);

		if (SUCCEEDED(hr)) {
			hr = pDWriteCreateFactory(
				DWRITE_FACTORY_TYPE_SHARED,
				__uuidof(IDWriteFactory),
				reinterpret_cast<IUnknown**>(&pDWriteFactory_)
				);
		}

		if (SUCCEEDED(hr)) {
			hr = pDWriteFactory_->CreateTextFormat(
				CNJIni::GetSettings()->commentFontName.c_str(),
				NULL,
				CNJIni::GetSettings()->commentFontBold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				static_cast<FLOAT>(iFontSize_),
				L"ja-jp",
				&pTextFormat_
				);
		}

		if (SUCCEEDED(hr)) {
			hr = pTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		}

		if (SUCCEEDED(hr)) {
			hr = pTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
		}

		if (SUCCEEDED(hr)) {
			hr = pTextFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		}

		return hr;
	}

	HRESULT CreateDeviceResources() {
		HRESULT hr = S_OK;

		RECT rc;
		GetClientRect(hWnd_, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

		if (!pRT_)
		{
			const D2D1_PIXEL_FORMAT format = 
				D2D1::PixelFormat(
				DXGI_FORMAT_B8G8R8A8_UNORM,
				D2D1_ALPHA_MODE_PREMULTIPLIED);

			const D2D1_RENDER_TARGET_PROPERTIES properties = 
				D2D1::RenderTargetProperties(
				D2D1_RENDER_TARGET_TYPE_DEFAULT,
				format,
				0.0f, // default dpi
				0.0f, // default dpi
				D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE);

			COMPTR_TYPEDEF(ID3D10Device1);
			ID3D10Device1Ptr device;
			hr = pD3D10CreateDevice1(
			  0, // adapter
			  D3D10_DRIVER_TYPE_HARDWARE,
			  0, // reserved
			  D3D10_CREATE_DEVICE_BGRA_SUPPORT,
			  D3D10_FEATURE_LEVEL_10_0,
			  D3D10_1_SDK_VERSION,
			  &device);

			COMPTR_TYPEDEF(ID3D10Texture2D);
			ID3D10Texture2DPtr texture;
			if (SUCCEEDED(hr)) {
				D3D10_TEXTURE2D_DESC description = {};
				description.ArraySize = 1;
				description.BindFlags = 
					D3D10_BIND_RENDER_TARGET;
				description.Format = 
					DXGI_FORMAT_B8G8R8A8_UNORM;
				description.Width = 1920;
				description.Height = 1200;
				description.MipLevels = 1;
				description.SampleDesc.Count = 1;
				description.MiscFlags = 
					D3D10_RESOURCE_MISC_GDI_COMPATIBLE;

				hr = device->CreateTexture2D(
					&description,
					0,
					&texture);
			}

			COMPTR_TYPEDEF(IDXGISurface);
			IDXGISurfacePtr surface;
			if (SUCCEEDED(hr)) {
				hr = texture->QueryInterface(&surface);
			}

			if (SUCCEEDED(hr)) {
				hr = pD2DFactory_->CreateDxgiSurfaceRenderTarget(
					surface,
					&properties,
					&pRT_);
			}

			/*pD2DFactory_->CreateWicBitmapRenderTarget(
				bitmap_,
				properties,
				&pRT_);*/
			if (SUCCEEDED(hr)) {
				pRT_->QueryInterface(&pGDITarget_);
			}

			//pRT_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_ALIASED);

			if (SUCCEEDED(hr)) {
				hr = pRT_->CreateSolidColorBrush(
					D2D1::ColorF(D2D1::ColorF::Black),
					&pBlackBrush_
					);
			}
			
			if (SUCCEEDED(hr)) {
				renderer_ = new CustomTextRenderer(
					pD2DFactory_.GetInterfacePtr(), pRT_.GetInterfacePtr(), pBlackBrush_.GetInterfacePtr()
					);
				hr = renderer_->QueryInterface(__uuidof(IDWriteTextRenderer), (void**)&pTextRenderer_);
			}
		}
		return hr;
	}

	void DiscardDeviceResources() {
		pTextRenderer_ = NULL;
		pBlackBrush_ = NULL;
		pRT_ = NULL;
	}

	SIZE GetTextSize(const wchar_t *text, int len) {
		SIZE size;
		HDC dc = GetDC(hWnd_);
		HGDIOBJ old = SelectObject(dc, hFont_);
		BOOL ret = GetTextExtentPoint32W(dc, text, len, &size);
		SelectObject(dc, old);
		ReleaseDC(hWnd_, dc);
		if (ret) {
			return size;
		}
		return SIZE();
	}

	RECT rcWnd_;
	int yPitch_;
	HRESULT hr;
	void Begin(RECT rcWnd, int yPitch) {
		CopyRect(&rcWnd_, &rcWnd);
		yPitch_ = yPitch;
		hr = CreateDeviceResources();

		if (SUCCEEDED(hr))
		{
			pRT_->BeginDraw();
			pRT_->SetTransform(D2D1::IdentityMatrix());
			pRT_->Clear(D2D1::ColorF(D2D1::ColorF::White, 0.0f));
		}
	}

	void End() {
		RECT rc;
		GetWindowRect(hWnd_, &rc);

		HDC dc;
		pGDITarget_->GetDC(
			D2D1_DC_INITIALIZE_MODE_COPY,
			&dc);

		POINT zero = {};
		SIZE size = {rc.right - rc.left, rc.bottom - rc.top};
		BLENDFUNCTION blend = { AC_SRC_OVER, 0, 0xff, AC_SRC_ALPHA };
		BOOL ret = UpdateLayeredWindow(hWnd_, NULL, NULL, &size, dc, &zero, 0, &blend, ULW_ALPHA);
		RECT nullRect = {};
		pGDITarget_->ReleaseDC(&nullRect);

		if (SUCCEEDED(hr)) {
			hr = pRT_->EndDraw(
				);
		}
		if (FAILED(hr)) {
			DiscardDeviceResources();
		}
	}

	void DrawShita(const Chat &chat) {
		RECT rc;
		rc.top = rcWnd_.bottom - yPitch_ - static_cast<LONG>(chat.line * (int)((float)yPitch_ * CNJIni::GetSettings()->commentLineMargin));
		rc.bottom = rc.top + yPitch_;
		rc.left = 0;
		rc.right = rcWnd_.right;
		D2D1_RECT_F layoutRect = D2D1::RectF(
			static_cast<FLOAT>(rc.left) / dpiScaleX_,
			static_cast<FLOAT>(rc.top) / dpiScaleY_,
			static_cast<FLOAT>(rc.right) / dpiScaleX_,
			static_cast<FLOAT>(rc.bottom) / dpiScaleY_
			);

		pTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		IDWriteTextLayout *pLayout;
		pDWriteFactory_->CreateTextLayout(
			chat.text.c_str(),
			chat.text.length(),
			pTextFormat_,
			static_cast<FLOAT>(rc.right) / dpiScaleX_,
			static_cast<FLOAT>(yPitch_) / dpiScaleY_,
			&pLayout
			);

		D2D1_POINT_2F pt = {static_cast<FLOAT>(rc.left) / dpiScaleX_, static_cast<FLOAT>(rc.top) / dpiScaleY_};
		ID2D1SolidColorBrush *pColor;
		pRT_->CreateSolidColorBrush(
			D2D1::ColorF(bgr2rgb(chat.color)),
			&pColor
			);
		renderer_->SetColorBrush(pColor);
		pLayout->Draw(NULL, pTextRenderer_, pt.x, pt.y);
		pColor->Release();
		pLayout->Release();
		pTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	}

	void DrawNormal(const Chat &chat, int vpos) {
		int x = static_cast<int>(rcWnd_.right - (float)(chat.width + rcWnd_.right) * (vpos - chat.vpos) / VPOS_LEN);
		int y = static_cast<int>(((float)yPitch_ * CNJIni::GetSettings()->commentLineMargin) * (float)chat.line);
		IDWriteTextLayout *pLayout;
		pDWriteFactory_->CreateTextLayout(chat.text.c_str(), chat.text.length(), pTextFormat_, 1920, 1200, &pLayout);

		D2D1_POINT_2F pt = {static_cast<FLOAT>(x) / dpiScaleX_, static_cast<FLOAT>(y) / dpiScaleY_};
		ID2D1SolidColorBrush *pColor;
		pRT_->CreateSolidColorBrush(
			D2D1::ColorF(bgr2rgb(chat.color)),
			&pColor
			);
		renderer_->SetColorBrush(pColor);
		pLayout->Draw(NULL, pTextRenderer_, pt.x, pt.y);
		pColor->Release();
		pLayout->Release();
	}
};

// static vars
DWPrinter::D2D1CreateFactoryPtr DWPrinter::pD2D1CreateFactory;
DWPrinter::DWriteCreateFactoryPtr DWPrinter::pDWriteCreateFactory;
DWPrinter::PFN_D3D10_CREATE_DEVICE1 DWPrinter::pD3D10CreateDevice1;
int DWPrinter::isAvailable = -1;

Printer *CreatePrinter(bool disableDW) {
	if (!disableDW && DWPrinter::IsAvailable()) {
		return new DWPrinter;
	}
	return new GDIPrinter;
}
