#include "stdafx.h"
#include "extStuff.h"

using namespace std;
using namespace cv;

void googleSearch( cv::Mat& img )
{
	imwrite( "./temp.png", img );
	setCursor( cursor_Busy );

	// spawn a process
	//wstring filename( L"curl.exe" );
	wchar_t args[] = L"curl.exe -F \"encoded_image=@temp.png;type=image/png\" \
-F \"image-url=\" -F \"image_content=\" -F \"filename=\" -F \"num=10\" \
-F \"hl=ru\" --insecure -D headers.txt https://www.google.com/searchbyimage/upload";

	PROCESS_INFORMATION lp;
	STARTUPINFO si;
	memset( &lp, 0, sizeof( lp ) );
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );
	if ( !CreateProcess( NULL, args, NULL, NULL, 0, 
			0, NULL, NULL, &si, &lp ) )
	{
		// fail
		MessageBox( NULL, L"Could not launch curl.exe", L"Unable to upload image", MB_ICONINFORMATION );
	}
	else
	{
		WaitForSingleObject( lp.hProcess, INFINITE );

		ifstream ifs;
		ifs.open( L"headers.txt" );
		
		string header;
		while ( !ifs.eof() ) {
			ifs >> header;
			if ( header != "Location:" ) continue;
			else {
				// found!
				ifs >> header;
				ShellExecute( NULL, L"open", toUtf16( header ).c_str(), 
					NULL, NULL, SW_SHOWNORMAL );
				break;
			}
		}

	}

	setCursor( cursor_Normal );
}

void syserror()
{
	unsigned dwError;
	dwError = GetLastError();
	void *msg;
	if ( FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 
		NULL, dwError, 0, (LPWSTR)&msg, 255, NULL ) )
	{
		cerr << "Failed to open clipboard (" << dwError << ") " << 
			fromUtf16(wstring((wchar_t*)msg)) << endl;
		LocalFree( (HLOCAL)msg );
	}

}

void imgFromClipboard( cv::Mat& img )
{
	if (IsClipboardFormatAvailable(CF_DIB))
	{
		// a DIB is in the clipboard, draw it out
		GLOBALHANDLE    hGMem ;
		LPBITMAPINFO    lpBI ;
		void*            pDIBBits;

		if ( !OpenClipboard( NULL ) ) {
			syserror();
			return;
		}

		hGMem = GetClipboardData(CF_DIB) ;
		if ( !hGMem ) {
			syserror();
			CloseClipboard();
			return;
		}

		lpBI = (LPBITMAPINFO)GlobalLock(hGMem) ;
		if ( !lpBI ) {
			syserror();
			CloseClipboard();
			return;
		}

		// point to DIB bits after BITMAPINFO object
		if ( lpBI->bmiHeader.biBitCount == 24 || lpBI->bmiHeader.biBitCount == 32 )
		{
			pDIBBits = ((char*)lpBI) + lpBI->bmiHeader.biSize + 
				lpBI->bmiHeader.biClrUsed * lpBI->bmiHeader.biBitCount / 8;
			Mat temp;
			temp = Mat( lpBI->bmiHeader.biHeight, 
				lpBI->bmiHeader.biWidth,
				lpBI->bmiHeader.biBitCount == 24 ? CV_8UC3 : CV_8UC4,
				pDIBBits );
			//temp.convertTo( img, CV_8UC3 );
			img = temp.clone();
			//cvtColor( img, img, CV_RGB2BGR );
			flip( img, img, 0 );
		}

		GlobalUnlock(hGMem) ;
		CloseClipboard() ;
	}
}


void setCursor( CursorType cursorType )
{
	switch( cursorType ) {
		case cursor_Normal: SetCursor( LoadCursor( NULL, IDC_ARROW ) ); break;
		case cursor_Busy: SetCursor( LoadCursor( NULL, IDC_WAIT ) ); break;
		case cursor_DragHorz: SetCursor( LoadCursor( NULL, IDC_SIZEWE ) ); break;
		case cursor_DragVert: SetCursor( LoadCursor( NULL, IDC_SIZENS ) ); break;
		case cursor_Create: SetCursor( LoadCursor( NULL, IDC_CROSS ) ); break;
	}
}