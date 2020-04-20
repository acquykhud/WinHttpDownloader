#include "AsynchronousWinHttp.h"

#pragma region AsynchronousWinHttp
AsynchronousWinHttp::AsynchronousWinHttp()
{
	m_qwByteReadCount = 0;
	m_percent = 0.f;
	m_bHeaderReady = FALSE;
	m_lpInternalBuffer = new BYTE[8192 + 2048]; // WinHttpReadData read maximum of 8KB data (don't know why I give it 2KB more)
	m_fnReadFunc = NULL;
	m_ctx = NULL;
}
AsynchronousWinHttp::~AsynchronousWinHttp()
{
	/*
		Close connection.
		Deallocate dynamic-allocated memory.
	*/
	close(); // In case user forgets to close
	if (m_lpInternalBuffer)
	{
		delete[] m_lpInternalBuffer;
		m_lpInternalBuffer = NULL;
	}
}
bool AsynchronousWinHttp::init()
{
	/*
		Open new session.
	*/
	m_hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/81.0.4044.113 Safari/537.36", 0, 0, 0, WINHTTP_FLAG_ASYNC); // TODO: agent
	return m_hSession != NULL;
}
void AsynchronousWinHttp::close()
{
	/*
		Close all HINTERNET handle.
	*/
	if (m_bIsClosed == TRUE)
	{
		return;
	}

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
	m_bIsClosed = TRUE;
	// Utils::info(L"[+] close called\n");
	std::lock_guard<std::mutex> lock(m_mutex);
	m_con.notify_one();
}
bool AsynchronousWinHttp::get(LPCWSTR szUrl, const std::wstring& sHeader)
{
	/*
		Send get request, headers=headers.
	*/
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

	if (!WinHttpSendRequest(m_hRequest, header, -1, NULL, 0, 0, (DWORD_PTR)this))
	{
		// TODO: cleanup
		return false;
	}

	return true;
}
bool AsynchronousWinHttp::wait()
{
	/*
		Synchronorous function wait.
	*/
	std::unique_lock<std::mutex> uLock(m_mutex); // lock 
	m_con.wait(uLock, [this] 
	{
		return this->m_hRequest == NULL; // when we closed the connection, this will be null
	});
	return true;
}
std::wstring AsynchronousWinHttp::getRawHeader() const
{
	/*
		Get response header.
	*/
	if (m_bHeaderReady)
		return m_sHeader;
	return std::wstring(L"");
}
bool AsynchronousWinHttp::getHeader()
{
	/*
		Get response header.
	*/
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
	if (dwBufLen = sizeof(m_qwRemoteFileSize), !WinHttpQueryHeaders(m_hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER64, NULL, &m_qwRemoteFileSize, &dwBufLen, NULL))
	{
		if (GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND)
			m_qwRemoteFileSize = 0;
	}
	if (dwBufLen = sizeof(szBytes), !WinHttpQueryHeaders(m_hRequest, WINHTTP_QUERY_ACCEPT_RANGES, NULL, szRecv, &dwBufLen, NULL))
	{
		if (GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND)
		{
			m_bSupportResuming = FALSE;
			int status;
			dwBufLen = sizeof(status);
			if (WinHttpQueryHeaders(m_hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &dwBufLen, NULL))
			{
				if (status == 206)
					m_bSupportResuming = TRUE;
			}
		}
	}
	else
		m_bSupportResuming = TRUE;
	m_sHeader = std::wstring(lpRecv);
	delete[] lpRecv;
	m_bHeaderReady = TRUE;

	return true;
}
bool AsynchronousWinHttp::isDataAvail()
{
	/*
		Check if we can read data now.
	*/
	if (!WinHttpQueryDataAvailable(m_hRequest, NULL))
	{
		// TODO: cleanup
		return false;
	}
	return true;
}
void AsynchronousWinHttp::update()
{
	/*
		Update progress.
	*/
	if (m_qwRemoteFileSize == 0) // percentage update: warning: divide by zero
	{
		return;
	}
	static auto lastTimeStamp = std::chrono::high_resolution_clock::now();
	static DWORD64 lastByteReadCount = m_qwByteReadCount;
	auto timeStamp = std::chrono::high_resolution_clock::now();
	auto timeStampDiff = std::chrono::duration_cast<std::chrono::milliseconds>(timeStamp - lastTimeStamp).count();
	auto byteReadCount = m_qwByteReadCount;
	if (timeStampDiff > 1000)
	{
		lastTimeStamp = timeStamp;
		float speed = (float)(byteReadCount - lastByteReadCount) / 1000;
		Utils::info(L"[+] %.2fkb/s\n", speed);
		lastByteReadCount = byteReadCount;
	}
}
void AsynchronousWinHttp::readBufferedData(DWORD size)
{
	/*
		Request to read data.
	*/
	if (!size)
		return;
	WinHttpReadData(m_hRequest, m_lpInternalBuffer, size, NULL);
}
BOOL AsynchronousWinHttp::getRemoteSize(DWORD64* lpDwSizeOut) const
{
	/*
		Get file size.
	*/
	if (m_bHeaderReady)
	{
		*lpDwSizeOut = m_qwRemoteFileSize;
		return TRUE;
	}
	return FALSE;
}
BOOL AsynchronousWinHttp::checkIfSupportResuming(LPBOOL lpBOut) const
{
	/*
		Check if server support resuming.
	*/
	if (m_bHeaderReady)
	{
		*lpBOut = m_bSupportResuming;
		return TRUE;
	}
	return FALSE;
}
void __stdcall AsynchronousWinHttp::WinhttpStatusCallback(IN HINTERNET hInternet, IN DWORD_PTR dwContext, IN DWORD dwInternetStatus, IN LPVOID lpvStatusInformation, IN DWORD dwStatusInformationLength)
{
	/*
		Callback function.
		The heart of this program is here :)

	*/
	AsynchronousWinHttp* lpCtx = (AsynchronousWinHttp*)dwContext;
	WINHTTP_ASYNC_RESULT* pWAR = (WINHTTP_ASYNC_RESULT*)lpvStatusInformation;
	switch (dwInternetStatus)
	{
	case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
		// Utils::info(L"[+] %s %s\n", lpCtx->m_sName.c_str(), L"WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE");
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
			// Got headers now, print to check !
			// Utils::info(L"[+] Header: \"\"\"\n%s\"\"\"\n", lpCtx->m_sHeader.c_str());
			// Utils::info(L"[+] RemoteSize: %lld\n", lpCtx->m_qwRemoteFileSize);
			// Utils::info(L"[+] Resumable: %s\n", lpCtx->m_bSupportResuming ? L"TRUE" : L"FALSE");
		}
		if (!lpCtx->isDataAvail())					   // Generate DATA_AVAILABLE
		{
			// TODO: cleanup
		}
		
		break;

	case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
	{
		DWORD size = *((LPDWORD)lpvStatusInformation);
		//Utils::info(L"[+] %u bytes available\n", size);
		if (size == 0)
		{
			// No more data to read, clean up now
			//Utils::info(L"[+] No more data, bye (%s)\n", lpCtx->m_sName.c_str());
			//Utils::info(L"[+] Total read: %lld\n", lpCtx->m_qwByteReadCount);
			lpCtx->close(); // 
		}
		else
		{
			lpCtx->readBufferedData(size);
		}
		break;
	}

	case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
	{
		DWORD size = dwStatusInformationLength;
		LPBYTE lpBuffer = (LPBYTE)lpvStatusInformation;
		lpCtx->m_qwByteReadCount += size;

		// lpCtx->update();
		if (lpCtx->m_fnReadFunc != NULL && lpCtx->m_ctx != NULL)
		{
			lpCtx->m_fnReadFunc(lpCtx->m_ctx, lpBuffer, size);
		}
		if (!lpCtx->isDataAvail())
		{
			// TODO: cleanup
		}
		break;
	}


	case WINHTTP_CALLBACK_STATUS_REDIRECT:
		/*
			I don't know what to do
		*/
		break;

	case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
	{
		//Utils::info(L"[+] WINHTTP_CALLBACK_STATUS_REQUEST_ERROR\n");
		Utils::info(L"[+] Error number: %d, error id: %d\n", pWAR->dwError, pWAR->dwResult);
		lpCtx->close();
		break;
	}

	default:
		break;
	}
}
#pragma endregion