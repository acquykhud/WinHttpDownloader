#pragma once
#include <Windows.h>
#include <tchar.h>
#include <winhttp.h>
#include <stdio.h>
#include <string>
#pragma comment(lib, "winhttp.lib")

class AsynchronousWinHttp
{
public:
	AsynchronousWinHttp();
	~AsynchronousWinHttp();

	bool init();
	void close();
	bool get(LPCWSTR szUrl, const std::wstring& sHeader);


	bool checkDataAvail();
	bool readDataAndWriteFile();

	void testRead(DWORD size);
	void setName(const std::wstring& name) { m_sName = name; }

	std::wstring getRawHeader() const;
	BOOL getRemoteSize(LPDWORD lpDwSizeOut) const;
	BOOL checkIfSupportResuming(LPBOOL lpBOut) const;

protected:
	HINTERNET m_hSession;
	HINTERNET m_hConnect;
	HINTERNET m_hRequest;

	BOOL m_bHeaderReady;

	DWORD m_dwRemoteFileSize;
	BOOL m_bSupportResuming;
	DWORD m_dwByteReadCount;
	std::wstring m_sHeader;
	std::wstring m_sName;

	bool getHeader(); 
protected:
	static void __stdcall WinhttpStatusCallback(IN HINTERNET hInternet, IN DWORD_PTR dwContext, IN DWORD dwInternetStatus, IN LPVOID lpvStatusInformation, IN DWORD dwStatusInformationLength);
};