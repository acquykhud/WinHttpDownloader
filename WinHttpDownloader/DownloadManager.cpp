#include "DownloadManager.h"

DownloadManager::DownloadManager(const std::wstring & url, DWORD nThread, DWORD nConn, BOOL resume)
{
	m_conFig.sUrl = url; m_conFig.dwThread = nThread; m_conFig.dwConn = nConn; m_conFig.bResume = resume;
	this->queryOptions();
	if (m_qwRemoteFileSize == 0uLL)
	{
		return;
	}
	if (m_bSupportResuming == FALSE)
	{
		// ?? use 1 thread
		return;
	}
	m_pSegmentFactory = new SegmentFactory(m_qwRemoteFileSize);
}

DownloadManager::~DownloadManager()
{
	delete m_pSegmentFactory;
}

DWORD64 DownloadManager::getFileSize() const
{
	return m_qwRemoteFileSize;
}

BOOL DownloadManager::supportResuming() const
{
	return m_bSupportResuming;
}

void DownloadManager::start()
{
	for (DWORD i = 0; i < m_conFig.dwThread; ++i)
	{
		m_threads.push_back(
			std::thread(&DownloadManager::downloadThread, this, m_conFig.dwConn)
		); // compiler generate: -> DownloadManager::download(void* this_ptr, int conn);
	}
	for (DWORD i = 0; i < m_conFig.dwThread; ++i)
	{
		m_threads[i].join();
	}
}

void DownloadManager::queryOptions()
{
	WCHAR szHost[256];
	WCHAR szBytes[] = L"Bytes";
	URL_COMPONENTS urlComp;
	HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
	DWORD dwBufLen;
	ZeroMemory(&urlComp, sizeof(urlComp));
	urlComp.dwStructSize = sizeof(urlComp);

	urlComp.lpszHostName = szHost;
	urlComp.dwHostNameLength = (sizeof(szHost) / sizeof(szHost[0]));
	urlComp.dwUrlPathLength = -1;
	urlComp.dwSchemeLength = -1;

	if (!WinHttpCrackUrl(m_conFig.sUrl.c_str(), 0, 0, &urlComp))
	{
		goto clean;
	}
	hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/81.0.4044.113 Safari/537.36", 0, NULL, NULL, 0);
	if (hSession == NULL)
	{
		goto clean;
	}
	hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, NULL);
	if (hConnect == NULL)
	{
		goto clean;
	}
	hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.lpszUrlPath, NULL, NULL, NULL, (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (hRequest == NULL)
	{
		goto clean;
	}
	if (!WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, NULL))
	{
		goto clean;
	}
	if (!WinHttpReceiveResponse(hRequest, NULL))
	{
		goto clean;
	}
	dwBufLen = sizeof(m_qwRemoteFileSize);
	if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER64, NULL, &m_qwRemoteFileSize, &dwBufLen, NULL))
		m_qwRemoteFileSize = 0;
	dwBufLen = sizeof(szBytes);
	if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_ACCEPT_RANGES, NULL, szBytes, &dwBufLen, NULL))
		m_bSupportResuming = FALSE;
	else
		m_bSupportResuming = TRUE;

clean:
	if (hRequest != NULL)
		WinHttpCloseHandle(hRequest);
	if (hConnect != NULL)
		WinHttpCloseHandle(hConnect);
	if (hSession != NULL)
		WinHttpCloseHandle(hSession);

	return;
}

void DownloadManager::downloadThread(int conn)
{
	std::vector<AsynchronousWinHttp*> workers;
	std::vector<HANDLE> files;
	for (int i = 0; i < conn; ++i)
	{
		/*
			Init connections
		*/
		Range range = this->m_pSegmentFactory->getNextSegment();
		if (range.ord == MAXDWORD64) // this segment is not valid
			continue;
		std::wstring header = L"Range: bytes=";
		header += std::to_wstring(range.start);
		header += L"-";
		header += std::to_wstring(range.end);
		header += L"\r\n";
		HANDLE hFile = CreateFileW((std::wstring(L"C:\\Users\\xikhud\\Desktop\\Out\\") + std::to_wstring(range.ord)).c_str()
			,GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		AsynchronousWinHttp* worker = new AsynchronousWinHttp;
		workers.push_back(worker);
		files.push_back(hFile);
		worker->setCtx((void*)hFile);
		worker->setReadFunc(DownloadManager::saveFile);
		worker->init();
		worker->get(m_conFig.sUrl.c_str(), header);
	}

	if (!workers.empty())
	{
		while (m_pSegmentFactory->isDataAvail())
		{
			/*
				Check for free connection.
				If found one, use it to download next segment
			*/
			for (int i = 0; i < (int)workers.size(); ++i)
			{
				if (workers[i] && workers[i]->isClosed())
				{
					delete workers[i];
					CloseHandle(files[i]);
					Range range = this->m_pSegmentFactory->getNextSegment();
					if (range.ord == MAXDWORD64) // this segment is not valid
						continue;
					std::wstring header = L"Range: bytes=";
					header += std::to_wstring(range.start);
					header += L"-";
					header += std::to_wstring(range.end);
					header += L"\r\n";
					HANDLE hFile = CreateFileW((std::wstring(L"C:\\Users\\xikhud\\Desktop\\Out\\") + std::to_wstring(range.ord)).c_str()
						, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
					AsynchronousWinHttp* worker = new AsynchronousWinHttp;
					workers[i] = worker;
					files[i] = hFile;
					worker->setCtx((void*)hFile);
					worker->setReadFunc(DownloadManager::saveFile);
					worker->init();
					worker->get(m_conFig.sUrl.c_str(), header);
				}
			}
			std::this_thread::sleep_for(std::chrono::seconds(1)); /* Sleep 1 second before checking for new segment */
		}
	}
	for (DWORD i = 0; i < workers.size(); ++i)
	{
		workers[i]->wait();
	}
	for (DWORD i = 0; i < workers.size(); ++i)
	{
		delete workers[i];
		CloseHandle(files[i]);
	}
}

void __stdcall DownloadManager::saveFile(void * lpCtx, LPBYTE lpData, DWORD nCount)
{
	DWORD w = 0;
	WriteFile((HANDLE)lpCtx, lpData, nCount, &w, NULL);
}


DownloadManager::SegmentFactory::SegmentFactory(DWORD64 qwFileSize)
{
	if (qwFileSize)
		m_qwMaxSegmentCount = (qwFileSize + (qwSegmentSize - 1)) / qwSegmentSize;
	else
		m_qwMaxSegmentCount = MAXDWORD64;
	m_qwSegmentCount = 0uLL;
	m_qwFileSize = qwFileSize;
}

Range DownloadManager::SegmentFactory::getNextSegment()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	Range range;
	range.start = m_qwSegmentCount * qwSegmentSize;
	range.ord = m_qwSegmentCount;
	if (m_qwSegmentCount != m_qwMaxSegmentCount - 1)
		range.end = range.start + (qwSegmentSize - 1);
	else
		range.end = range.start + (m_qwFileSize % qwSegmentSize) - 1;
	m_qwSegmentCount += 1uLL;
	if (m_qwSegmentCount > m_qwMaxSegmentCount)
		range.ord = MAXDWORD64;
	return range;
}
