#include "stdafx.h"

#include "WinCtrl.h"
#include "convertor.h"

#include "json.hpp"
using JSON = nlohmann::json;

std::wstring WindowsControl::GetWindowList()
{
	JSON json;
	BOOL bResult = ::EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL
		{
			if (IsWindowVisible(hWnd)) {
				JSON j;
				j["hWnd"] = (INT64)hWnd;

				WCHAR buffer[256];
				::GetClassName(hWnd, buffer, 256);
				j["class"] = WC2MB(buffer, CP_UTF8);

				int length = GetWindowTextLength(hWnd);
				if (length != 0) {
					std::wstring text;
					text.resize(length);
					::GetWindowText(hWnd, &text[0], length + 1);
					j["text"] = WC2MB(text, CP_UTF8);
				}

				DWORD dwProcessId;
				::GetWindowThreadProcessId(hWnd, &dwProcessId);
				j["pid"] = dwProcessId;

				JSON* json = (JSON*)lParam;
				json->push_back(j);
			}
			return TRUE;
		}, (LPARAM)&json);

	return MB2WC(json.dump(), CP_UTF8);
}

HWND WindowsControl::ActiveWindow()
{
	return GetActiveWindow();
}

HWND WindowsControl::CurrentWindow()
{
	DWORD pid = GetCurrentProcessId();
	std::pair<HWND, DWORD> params = { 0, pid };

	// Enumerate the windows using a lambda to process each window
	BOOL bResult = ::EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL
		{
			auto pParams = (std::pair<HWND, DWORD>*)(lParam);
			WCHAR buffer[256];
			DWORD processId;
			if (IsWindowVisible(hWnd)
				&& ::GetWindowThreadProcessId(hWnd, &processId)
				&& processId == pParams->second
				&& ::GetClassName(hWnd, buffer, 256)
				&& wcscmp(L"V8TopLevelFrameSDI", buffer) == 0
				) {
				// Stop enumerating
				SetLastError(-1);
				pParams->first = hWnd;
				return FALSE;
			}

			// Continue enumerating
			return TRUE;
		}, (LPARAM)&params);

	if (!bResult && GetLastError() == -1 && params.first)
	{
		return params.first;
	}

	return 0;
}

BOOL WindowsControl::SetWindowSize(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 3) return false;
	HWND hWnd = VarToHwnd(paParams);
	int w = VarToInt(paParams + 1);
	int h = VarToInt(paParams + 2);
	::SetWindowPos(hWnd, 0, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);
	::UpdateWindow(hWnd);
	return true;
}

BOOL WindowsControl::SetWindowPos(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 3) return false;
	HWND hWnd = VarToHwnd(paParams);
	int x = VarToInt(paParams + 1);
	int y = VarToInt(paParams + 1);
	::SetWindowPos(hWnd, 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
	::UpdateWindow(hWnd);
	return true;
}

BOOL WindowsControl::EnableResizing(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 2) return false;
	HWND hWnd = VarToHwnd(paParams);
	BOOL enable = VarToInt(paParams + 1);
	LONG style = ::GetWindowLong(hWnd, GWL_STYLE);
	style = enable ? (style | WS_SIZEBOX) : (style & ~WS_SIZEBOX);
	::SetWindowLong(hWnd, GWL_STYLE, style);
	::UpdateWindow(hWnd);
	return true;
}

std::wstring WindowsControl::GetText(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 1) return false;
	HWND hWnd = VarToHwnd(paParams);
	const int length = ::GetWindowTextLength(hWnd);
	std::wstring text;
	if (length != 0) {
		text.resize(length);
		::GetWindowText(hWnd, &text[0], length + 1);
	}
	return text;
}

BOOL WindowsControl::SetText(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 2) return false;
	HWND hWnd = VarToHwnd(paParams);
	return ::SetWindowText(hWnd, (paParams + 1)->pwstrVal);
}

#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")

BOOL WindowsControl::CaptureScreen(tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{
	const int mode = lSizeArray == 0 ? 0 : VarToInt(paParams);
	if (mode == 2) return CaptureWindow(pvarRetValue, 0);

	HWND hWnd = ::GetForegroundWindow();
	UpdateWindow(hWnd);

	LONG x, y, w, h;
	if (mode == 1) {
		RECT rect;
		DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect));
		x = rect.left;
		y = rect.top;
		w = rect.right - rect.left;
		h = rect.bottom - rect.top;
	}
	else {
		x = 0;
		y = 0;
		w = GetSystemMetrics(SM_CXSCREEN);
		h = GetSystemMetrics(SM_CYSCREEN);
	}

	HDC hScreen = GetDC(NULL);
	HDC hDC = CreateCompatibleDC(hScreen);
	HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, w, h);
	HGDIOBJ object = SelectObject(hDC, hBitmap);
	BitBlt(hDC, 0, 0, w, h, hScreen, x, y, SRCCOPY);
	const BOOL result = SaveBitmap(hBitmap, pvarRetValue);

	SelectObject(hDC, object);
	DeleteDC(hDC);
	ReleaseDC(NULL, hScreen);
	DeleteObject(hBitmap);

	return result;
}

BOOL WindowsControl::CaptureWindow(tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{
	HWND hWnd = 0;
	if (lSizeArray > 0) hWnd = VarToHwnd(paParams);
	return CaptureWindow(pvarRetValue, hWnd);
}

BOOL WindowsControl::CaptureWindow(tVariant* pvarRetValue, HWND hWnd)
{
	if (hWnd == 0) hWnd = ::GetForegroundWindow();

	RECT rc;
	GetClientRect(hWnd, &rc);
	DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc, sizeof(rc));

	HDC hdcScreen = GetDC(NULL);
	HDC hDC = CreateCompatibleDC(hdcScreen);
	HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, rc.right - rc.left, rc.bottom - rc.top);
	SelectObject(hDC, hBitmap);
	PrintWindow(hWnd, hDC, 0);

	const BOOL result = SaveBitmap(hBitmap, pvarRetValue);

	ReleaseDC(NULL, hdcScreen);
	DeleteDC(hDC);
	DeleteObject(hBitmap);

	return result;
}

#include <gdiplus.h>
#pragma comment(lib, "Gdiplus.lib")

class GgiPlusToken {
private:
	ULONG_PTR h = NULL;
public:
	GgiPlusToken() {}
	~GgiPlusToken() { if (h) Gdiplus::GdiplusShutdown(h); }
	ULONG_PTR* operator &() { return &h; }
	BOOL operator!() { return !h; }
};

static GgiPlusToken gdiplusToken;

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0) return -1;  // Failure

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL) return -1;  // Failure

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

BOOL WindowsControl::SaveBitmap(HBITMAP hBitmap, tVariant* pvarRetValue)
{
	BOOL Ret = FALSE;
	Gdiplus::Status status = Gdiplus::Ok;
	if (!gdiplusToken) // initialization of gdi+
	{
		const Gdiplus::GdiplusStartupInput input;
		status = Gdiplus::GdiplusStartup(&gdiplusToken, &input, NULL);
		if (status != Gdiplus::Ok) return false;
	}

	CLSID clsid;
	GetEncoderClsid(L"image/png", &clsid); // retrieving JPEG encoder CLSID

	Gdiplus::Bitmap SrcBitmap(hBitmap, 0); // creating bitmap

	IStream* pStream;
	if (SUCCEEDED(CreateStreamOnHGlobal(0, TRUE, &pStream))) // creating stream
	{
		status = SrcBitmap.Save(pStream, &clsid, 0); // saving image to the stream
		if (status == Gdiplus::Ok)
		{
			LARGE_INTEGER lOfs;
			ULARGE_INTEGER lSize;
			lOfs.QuadPart = 0;
			if (SUCCEEDED(pStream->Seek(lOfs, STREAM_SEEK_END, &lSize))) // retrieving size of stream data (seek to end)
			{
				lOfs.QuadPart = 0;
				if (SUCCEEDED(pStream->Seek(lOfs, STREAM_SEEK_SET, 0))) // seeking to beginning of the stream data
				{
					pvarRetValue->strLen = (ULONG)((DWORD_PTR)lSize.QuadPart);
					m_iMemory->AllocMemory((void**)&pvarRetValue->pstrVal, pvarRetValue->strLen);
					TV_VT(pvarRetValue) = VTYPE_BLOB;
					if (pvarRetValue->pstrVal)
					{
						if (SUCCEEDED(pStream->Read(pvarRetValue->pstrVal, pvarRetValue->strLen, 0))) // reading stream to buffer
						{
							Ret = TRUE;
						}
					}
				}
			}
		}
		pStream->Release(); // releasing stream
	}
	return Ret;
}

BOOL WindowsControl::Maximize(tVariant* paParams, const long lSizeArray)
{
	HWND hWnd = 0;
	if (lSizeArray > 0) hWnd = VarToHwnd(paParams);
	if (hWnd == 0) hWnd = ::GetForegroundWindow();
	if (IsWindow(hWnd)) {
		if (IsWindowVisible(hWnd)) {
			ShowWindow(hWnd, SW_SHOWMAXIMIZED);
		}
	}
	return true;
}

BOOL WindowsControl::Activate(tVariant* paParams, const long lSizeArray)
{
	if (lSizeArray < 1) return false;
	HWND hWnd = VarToHwnd(paParams);
	if (IsWindow(hWnd)) {
		if (IsWindowVisible(hWnd)) {
			WINDOWPLACEMENT place;
			memset(&place, 0, sizeof(WINDOWPLACEMENT));
			place.length = sizeof(WINDOWPLACEMENT);
			GetWindowPlacement(hWnd, &place);
			if (place.showCmd == SW_SHOWMINIMIZED) ShowWindow(hWnd, SW_RESTORE);
			SetForegroundWindow(hWnd);
		}
	}
	return true;
}
