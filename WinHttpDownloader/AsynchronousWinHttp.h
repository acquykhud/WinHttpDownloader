#ifndef ASYNCHRONOUSWINHTTP_H
#define ASYNCHRONOUSWINHTTP_H

#include <Windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#include <mutex>
#include <condition_variable>
#include "Utils.h"
#pragma comment(lib, "winhttp.lib")

class AsynchronousWinHttp
{
public:
	AsynchronousWinHttp();
	~AsynchronousWinHttp();

	bool init();
	void close();
	bool get(LPCWSTR szUrl, const std::wstring& sHeader);
	bool wait();

	bool isDataAvail();

	void update();
	BOOL isClosed() const { return m_bIsClosed; }

	void readBufferedData(DWORD size);
	void setName(const std::wstring& name) { m_sName = name; }
	
	void setReadFunc(void(__stdcall *fn)(void* ctx, LPBYTE, DWORD)) { this->m_fnReadFunc = fn; } // User-defined function to process data.
	void setCtx(void* ctx) { this->m_ctx = ctx; }												 // User-defined variable use to pass to "fn" (this->m_fnReadFunc(this-m_ctx, ...))

	std::wstring getRawHeader() const;					   // for debugging only
	BOOL getRemoteSize(DWORD64* lpQwSizeOut) const;		   // for debugging only
	BOOL checkIfSupportResuming(LPBOOL lpBOut) const;	   // for debugging only

private:
	HINTERNET m_hSession;
	HINTERNET m_hConnect;
	HINTERNET m_hRequest;

	void(__stdcall *m_fnReadFunc)(void* ctx, LPBYTE lpData, DWORD nCount);
	void* m_ctx;

	std::mutex m_mutex;			   // async, wait, ...
	std::condition_variable m_con; // async, wait, ...

	float m_percent;			   // for updating progress ??

	DWORD64 m_qwRemoteFileSize;	   // file size can be larger than 4GB 
	DWORD64 m_qwByteReadCount;	   // file size can be larger than 4GB 

	BOOL m_bSupportResuming;	   // support resuming is supported ? , don't use
	BOOL m_bHeaderReady;		   // header is ready to be read, don't use
	BOOL m_bIsClosed;              // is this connection closed ?

	std::wstring m_sHeader;		   // header to be sent in GET request
	std::wstring m_sName;		   // object name, for debugging

	PBYTE m_lpInternalBuffer;	   // store data from WinHttpReadData

	bool getHeader(); 


protected:
	static void __stdcall WinhttpStatusCallback(IN HINTERNET hInternet, IN DWORD_PTR dwContext, IN DWORD dwInternetStatus, IN LPVOID lpvStatusInformation, IN DWORD dwStatusInformationLength);
};

#endif