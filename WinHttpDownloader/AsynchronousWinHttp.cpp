#include "AsynchronousWinHttp.h"

AsynchronousWinHttp::AsynchronousWinHttp()
{
	m_dwByteReadCount = 0;
	m_bHeaderReady = FALSE;
}

AsynchronousWinHttp::~AsynchronousWinHttp()
{
	close();
}

bool AsynchronousWinHttp::init()
{
	m_hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/81.0.4044.113 Safari/537.36", 0, 0, 0, WINHTTP_FLAG_ASYNC); // TODO: agent
	return m_hSession != NULL;
}

void AsynchronousWinHttp::close()
{
	wprintf(L"[+] close called\n");
	if (m_hRequest)
	{
		WinHttpSetStatusCallback(m_hRequest, NULL, NULL, NULL);
		WinHttpCloseHandle(m_hRequest);
		m_hRequest = NULL;
	}
	if (m_hConnect)
	{
		WinHttpCloseHandle(m_hConnect);
		m_hConnect = NULL;
	}
	if (m_hSession)
	{
		WinHttpCloseHandle(m_hSession);
		m_hSession = NULL;
	}
}

bool AsynchronousWinHttp::get(LPCWSTR szUrl, const std::wstring& sHeader)
{
	WCHAR szHost[256];
	URL_COMPONENTS urlComp;
	ZeroMemory(&urlComp, sizeof(urlComp));
	urlComp.dwStructSize = sizeof(urlComp);

	urlComp.lpszHostName = szHost;
	urlComp.dwHostNameLength = (sizeof(szHost) / sizeof(szHost[0]));
	urlComp.dwUrlPathLength = -1;
	urlComp.dwSchemeLength = -1;

	if (!WinHttpCrackUrl(szUrl, 0, 0, &urlComp)) 
		return false;

	m_hConnect = WinHttpConnect(m_hSession, urlComp.lpszHostName, urlComp.nPort, NULL);
	if (!m_hConnect)
	{
		// TODO: cleanup
		return false;
	}

	m_hRequest = WinHttpOpenRequest(m_hConnect, L"GET", urlComp.lpszUrlPath, NULL, NULL, NULL,(urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!m_hRequest)
	{
		// TODO: cleanup
		return false;
	}

	WINHTTP_STATUS_CALLBACK pOldCallback = WinHttpSetStatusCallback(m_hRequest, (WINHTTP_STATUS_CALLBACK)WinhttpStatusCallback, WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_REDIRECT, NULL);
	if (pOldCallback != NULL)
	{
		// TODO: cleanup
		return false;
	}

	const WCHAR* header = (sHeader.length() == 0) ? NULL : sHeader.c_str();

	if (!!WinHttpSendRequest(m_hRequest, header, -1, NULL, 0, 0, (DWORD_PTR)this))
	{
		// TODO: cleanup
		return false;
	}
	printf("");
	return true;
}

std::wstring AsynchronousWinHttp::getRawHeader() const
{
	if (m_bHeaderReady)
		return m_sHeader;
	return std::wstring(L"");
}

bool AsynchronousWinHttp::getHeader()
{
	DWORD dwSize = 0;
	if (!WinHttpQueryHeaders(m_hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, 0, NULL, &dwSize, NULL))
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			// TODO: cleanup
			return false;
		}
	}
	WCHAR* lpRecv = new WCHAR[dwSize / sizeof(WCHAR)];
	if (!WinHttpQueryHeaders(m_hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, 0, lpRecv, &dwSize, NULL))
	{
		// TODO: cleanup
		delete[] lpRecv;
		return false;
	}
	DWORD dwBufLen;
	WCHAR szBytes[] = L"Bytes";
	WCHAR szRecv[sizeof(szBytes) / sizeof(WCHAR)];
	if (dwBufLen = sizeof(m_dwRemoteFileSize), !WinHttpQueryHeaders(m_hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, NULL, &m_dwRemoteFileSize, &dwBufLen, NULL))
	{
		if (GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND)
			m_dwRemoteFileSize = 0;
	}
	if (dwBufLen = sizeof(szBytes), !WinHttpQueryHeaders(m_hRequest, WINHTTP_QUERY_ACCEPT_RANGES, NULL, szRecv, &dwBufLen, NULL))
	{
		if (GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND)
		{
			m_bSupportResuming = FALSE;
			int status;
			dwBufLen = sizeof(status);
			WinHttpQueryHeaders(m_hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &dwBufLen, NULL);
			if (status == 206)
				m_bSupportResuming = TRUE;
		}
	}
	else
		m_bSupportResuming = TRUE;
	m_sHeader = std::wstring(lpRecv);
	delete[] lpRecv;
	m_bHeaderReady = TRUE;

	return true;
}

bool AsynchronousWinHttp::checkDataAvail()
{
	if (!WinHttpQueryDataAvailable(m_hRequest, NULL))
	{
		// TODO: cleanup
		return false;
	}
	return true;
}

void AsynchronousWinHttp::testRead(DWORD size)
{
	if (!size)
		return;
	char buf[10000];
	DWORD n;
	WinHttpReadData(m_hRequest, buf, size, &n);
	return;
}

BOOL AsynchronousWinHttp::getRemoteSize(LPDWORD lpDwSizeOut) const
{
	if (m_bHeaderReady)
	{
		*lpDwSizeOut = m_dwRemoteFileSize;
		return TRUE;
	}
	return FALSE;
}

BOOL AsynchronousWinHttp::checkIfSupportResuming(LPBOOL lpBOut) const
{
	if (m_bHeaderReady)
	{
		*lpBOut = m_bSupportResuming;
		return TRUE;
	}
	return FALSE;
}

void __stdcall AsynchronousWinHttp::WinhttpStatusCallback(IN HINTERNET hInternet, IN DWORD_PTR dwContext, IN DWORD dwInternetStatus, IN LPVOID lpvStatusInformation, IN DWORD dwStatusInformationLength)
{
	AsynchronousWinHttp* lpCtx = (AsynchronousWinHttp*)dwContext;
	WINHTTP_ASYNC_RESULT* pWAR = (WINHTTP_ASYNC_RESULT*)lpvStatusInformation;
	switch (dwInternetStatus)
	{
	case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
		//puts("[+] WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE");
		wprintf(L"[+] %s %s\n", lpCtx->m_sName.c_str(), L"WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE");
		if (!WinHttpReceiveResponse(lpCtx->m_hRequest, NULL)) // Generate HEADERS_AVAILABLE
		{
			// TODO: cleanup
		}
		break;

	case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
		if (!lpCtx->getHeader())
		{
			// TODO: cleanup
		}
		else
		{
			/* 
				Got header, do whatever   
			*/
			//wprintf(L"[+] Header: \"\"\"\n%s\"\"\"\n", lpCtx->m_sHeader.c_str());
			//wprintf(L"[+] RemoteSize: %d\n", lpCtx->m_dwRemoteFileSize);
			//wprintf(L"[+] Resumable: %s\n", lpCtx->m_bSupportResuming ? L"TRUE" : L"FALSE");
		}
		if (!lpCtx->checkDataAvail())					   // Generate DATA_AVAILABLE
		{
			// TODO: cleanup
		}
		
		break;

	case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
	{
		DWORD size = *((LPDWORD)lpvStatusInformation);
		//wprintf(L"[+] %u bytes available\n", size);
		if (size == 0)
		{
			// No more data to read, clean up now
			lpCtx->close();
			wprintf(L"[+] No more data, bye (%s)\n", lpCtx->m_sName.c_str());
			wprintf(L"[+] Total read: %d\n", lpCtx->m_dwByteReadCount);
		}
		else
		{
			lpCtx->testRead(size);
		}
		break;
	}

	case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
	{
		DWORD size = dwStatusInformationLength;
		//wprintf(L"[+] %u bytes had been read\n", size);
		lpCtx->m_dwByteReadCount += size;
		if (!lpCtx->checkDataAvail())
		{
			// TODO: cleanup
		}
		break;
	}


	case WINHTTP_CALLBACK_STATUS_REDIRECT:
		puts("[+] WINHTTP_CALLBACK_STATUS_REDIRECT");
		break;

	case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
	{
		puts("[+] WINHTTP_CALLBACK_STATUS_REQUEST_ERROR");
		wprintf(L"[+] Error number: %d, error id: %d\n", pWAR->dwError, pWAR->dwResult);
		lpCtx->close();
		break;
	}

	default:
		break;
	}
}
