#pragma once
#include <Windows.h>
#include <tchar.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#include <mutex>
#include <condition_variable>
#pragma comment(lib, "winhttp.lib")

class AsynchronousWinHttp
{
public:
	AsynchronousWinHttp();
	~AsynchronousWinHttp();

	bool init();
	void close();
	bool get(LPCWSTR szUrl, const std::wstring& sHeader);
	bool wait(DWORD dwTimeOut = INFINITE);

	bool isDataAvail();

	void update();
	BOOL isClosed() const { return m_bIsClosed; }

	void readBufferedData(DWORD size);
	void setName(const std::wstring& name) { m_sName = name; }
	
	void setReadFunc(void(__stdcall *fn)(void* ctx, LPBYTE, DWORD)) { this->m_fnReadFunc = fn; }
	void setCtx(void* ctx) { this->m_ctx = ctx; }

	std::wstring getRawHeader() const;
	BOOL getRemoteSize(DWORD64* lpQwSizeOut) const;
	BOOL checkIfSupportResuming(LPBOOL lpBOut) const;

private:
	HINTERNET m_hSession;
	HINTERNET m_hConnect;
	HINTERNET m_hRequest;

	void(__stdcall *m_fnReadFunc)(void* ctx, LPBYTE lpData, DWORD nCount);
	void* m_ctx;

	std::mutex m_mutex;
	std::condition_variable m_con;

	float m_percent;

	DWORD64 m_qwRemoteFileSize;	   // file size can be larger than 4GB 
	DWORD64 m_qwByteReadCount;	   // file size can be larger than 4GB 

	BOOL m_bSupportResuming;
	BOOL m_bHeaderReady;
	BOOL m_bIsClosed;              // is this connection closed ?

	std::wstring m_sHeader;
	std::wstring m_sName;

	PBYTE m_internalBuffer;

	bool getHeader(); 


protected:
	static void __stdcall WinhttpStatusCallback(IN HINTERNET hInternet, IN DWORD_PTR dwContext, IN DWORD dwInternetStatus, IN LPVOID lpvStatusInformation, IN DWORD dwStatusInformationLength);
};