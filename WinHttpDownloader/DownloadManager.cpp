#include "DownloadManager.h"

#define CONFIG_FILENAME L"Config"
#define INFO_FILENAME L"Info"
#define WINHTTPDOWNLOADER_FILENAME L"WinHttpDownloader"

DownloadManager::DownloadManager(const std::wstring & url, const std::wstring& sFullPath, DWORD nThread, DWORD nConn)
{
	/*
		Setup anything need to download file.
	*/
	m_conFig.sUrl = url;
	m_conFig.dwThread = nThread;
	m_conFig.dwConn = nConn;
	m_conFig.sFullPath = sFullPath;
	this->updateConfig();
	this->queryOptions();
	m_pSegmentFactory = new SegmentFactory(m_qwRemoteFileSize, m_conFig);
	if (m_conFig.bReady && m_conFig.bResume)
	{
		m_pSegmentFactory->repair();
	}
	else if (m_conFig.bReady /*&& m_conFig.bResume == FALSE */)
	{
		m_pSegmentFactory->writeInfo();
	}
}

DownloadManager::~DownloadManager()
{
	/*
		Deallocate dynamic-allocated memory.
		Remove leftovers.
	*/
	if (m_pSegmentFactory)
	{
		delete m_pSegmentFactory;
		m_pSegmentFactory = NULL;
	}
	cleanTmpFiles();
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
	/*
		Base on m_conFig, determine what to do next.
	*/
	if (m_qwRemoteFileSize == 0 || m_qwRemoteFileSize == MAXDWORD64 || m_bSupportResuming == FALSE)
	{
		Utils::info(L"[+] Not support resuming -> use 1 connection\n");
		// TODO: download using 1 thread,  1 connection
	}
	else if (m_conFig.bReady == FALSE)
	{
		Utils::info(L"[+] Something is wrong, please check --out arg\n");
	}
	else if (m_conFig.bResume == FALSE)
	{
		Utils::info(L"[+] Start downloading new file\n");
		writeConfig();
		startDownloading();
	}
	else // m_conFig.bResume == TRUE
	{
		Utils::info(L"[+] Resuming\n");
		startDownloading();
	}
}

void DownloadManager::startDownloading()
{
	/*
		Start workers.
		Wait for them to finish.
	*/
	for (DWORD i = 0; i < m_conFig.dwThread; ++i)
	{
		int nThread = m_conFig.dwConn / m_conFig.dwThread;
		if (i != m_conFig.dwThread - 1)
			m_threads.push_back(
				std::thread(&DownloadManager::downloadThread, this, nThread)
			); // compiler generate: -> DownloadManager::download(void* this_ptr, int conn);
		else
			m_threads.push_back(
				std::thread(&DownloadManager::downloadThread, this, nThread + (m_conFig.dwConn % m_conFig.dwThread))
			); // compiler generate: -> DownloadManager::download(void* this_ptr, int conn);
	}
	for (DWORD i = 0; i < m_conFig.dwThread; ++i)
	{
		m_threads[i].join();
	}
}

void DownloadManager::merge()
{
	/*
		Merge temp files.
		And remove them.
	*/
	FileMerger fm(m_conFig, m_pSegmentFactory->getMaxSegmentCount());
	fm.start();
}

void DownloadManager::cleanTmpFiles()
{
	/*
		Remove config and info files.
		Remove directory with hash named.
	*/
	DeleteFileW((m_conFig.sDirPath + CONFIG_FILENAME).c_str());
	DeleteFileW((m_conFig.sDirPath + INFO_FILENAME).c_str());
	RemoveDirectoryW(m_conFig.sDirPath.c_str());
}

void DownloadManager::queryOptions()
{
	/*
		Get file size.
		Check if server support resuming.
	*/
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
		m_qwRemoteFileSize = MAXDWORD64;
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
	Utils::info(L"[+] Conn = %d\n", conn);
	std::vector<AsynchronousWinHttp*> workers;
	std::vector<HANDLE> files;
	for (int i = 0; i < conn; ++i)
	{
		/*
			Init connections once.
		*/
		Range range = this->m_pSegmentFactory->getNextSegment();
		if (range.ord == MAXDWORD64) // ----------------->  this segment is not valid
			continue;
		std::wstring header = L"Range: bytes=";
		header += std::to_wstring(range.start);
		header += L"-";
		header += std::to_wstring(range.end);
		header += L"\r\n";
		HANDLE hFile = CreateFileW((m_conFig.sDirPath + std::to_wstring(range.ord)).c_str()
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
					if (range.ord == MAXDWORD64) // ----------------->  this segment is not valid
						continue;
					std::wstring header = L"Range: bytes=";
					header += std::to_wstring(range.start);
					header += L"-";
					header += std::to_wstring(range.end);
					header += L"\r\n";
					HANDLE hFile = CreateFileW((m_conFig.sDirPath + std::to_wstring(range.ord)).c_str()
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

void DownloadManager::updateConfig()
{
	/*
		This function, separate FullPath into filename and directory. 
		Calculate SHA256, and determine what to do.
	*/
	m_conFig.bReady = FALSE;
	auto p = m_conFig.sFullPath.rfind(L'\\');
	if (p == std::wstring::npos)
	{
		Utils::info(L"[+] Wrong path\n");
		return;
	}
	m_conFig.sFileName = m_conFig.sFullPath.substr(p + 1); // p+1 -> end

	std::wstring sTmp = m_conFig.sFullPath + m_conFig.sUrl;
	std::string sTmpEncoded = Utils::utf8_encode(sTmp);
	m_conFig.sSHA256 = Utils::sha256_hexdigest((LPCBYTE)sTmpEncoded.c_str(), sTmpEncoded.length());
	Utils::info(L"[+] %s:%s\n", sTmp.c_str(), m_conFig.sSHA256.c_str());

	m_conFig.sDirPath = Utils::getTempPath() + WINHTTPDOWNLOADER_FILENAME + L'\\' + m_conFig.sSHA256 + L'\\';
	
	std::wstring sPath = Utils::getTempPath() + WINHTTPDOWNLOADER_FILENAME + L'\\';
	CreateDirectoryW(sPath.c_str(), NULL); // Don't care
	sPath = m_conFig.sDirPath;

	Utils::info(L"[+] %s:%d\n", sPath.c_str(), sPath.length());
	m_conFig.bResume = FALSE;
	if (!CreateDirectoryW(sPath.c_str(), NULL))
	{
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			BOOL bResumeable = FALSE;
			std::wstring sTmp = m_conFig.sDirPath + CONFIG_FILENAME;
			HANDLE hFile = CreateFileW(sTmp.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				DWORD nr;
				ReadFile(hFile, &bResumeable, sizeof(bResumeable), &nr, NULL);
				CloseHandle(hFile);
			}
			m_conFig.bResume = bResumeable;
		}
	}
	Utils::info(L"[+] Resuming: %s\n", m_conFig.bResume ? L"TRUE" : L"FALSE");
	m_conFig.bReady = TRUE; 
}

void DownloadManager::writeConfig()
{
	/*
		Write config file.
	*/
	std::wstring sTmp = m_conFig.sDirPath + CONFIG_FILENAME;
	HANDLE hFile = CreateFileW(sTmp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		Utils::info(L"[+] Can't write config, last error = \n", GetLastError());
		return;
	}
	BOOL bResumeable = m_bSupportResuming;
	DWORD nw;
	WriteFile(hFile, &bResumeable, sizeof(bResumeable), &nw, NULL);
	CloseHandle(hFile);
}

void __stdcall DownloadManager::saveFile(void * lpCtx, LPBYTE lpData, DWORD nCount)
{
	/*
		Simple write...
	*/
	DWORD w = 0;
	WriteFile((HANDLE)lpCtx, lpData, nCount, &w, NULL);
}


DownloadManager::SegmentFactory::SegmentFactory(DWORD64 qwFileSize, const Config & config)
{
	/*
		Init SegmentFactory.
	*/
	m_qwFileSize = qwFileSize;
	m_sDirPath = config.sDirPath;
	m_sSHA256 = config.sSHA256;
	if (qwFileSize != MAXDWORD64)
		m_qwMaxSegmentCount = (qwFileSize + (qwSegmentSize - 1)) / qwSegmentSize;
	else
		m_qwMaxSegmentCount = MAXDWORD64;
	m_qwSegmentCount = 0uLL;
}

DownloadManager::SegmentFactory::~SegmentFactory()
{
	/*
		Close file which we use to update "latest segment".
	*/
	closeFile();
}

Range DownloadManager::SegmentFactory::getNextSegment()
{
	/*
		Synchronously serve next segment.
		Prevent workers from downloading same segment.
	*/
	std::lock_guard<std::mutex> lock(m_mutex);
	Range range;
	if (!m_uncompletedSegments.empty())
	{
		range = m_uncompletedSegments.back();
		m_uncompletedSegments.pop_back();
		return range;
	}
	range.start = m_qwSegmentCount * qwSegmentSize;
	range.ord = m_qwSegmentCount;
	if (m_qwSegmentCount != m_qwMaxSegmentCount - 1)
		range.end = range.start + (qwSegmentSize - 1);
	else
		range.end = range.start + (m_qwFileSize % qwSegmentSize) - 1;
	m_qwSegmentCount += 1uLL;
	updateLastestSegment(m_qwSegmentCount);
	if (m_qwSegmentCount > m_qwMaxSegmentCount)
		range.ord = MAXDWORD64;

	return range;
}

void DownloadManager::SegmentFactory::repair()
{
	/*
		Until now, we know that the server supports resuming.
	*/
	std::wstring sTmp = m_sDirPath + INFO_FILENAME;
	m_hFile = CreateFileW(sTmp.c_str(), GENERIC_READ | GENERIC_WRITE, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (m_hFile == INVALID_HANDLE_VALUE)
	{
		Utils::info(L"[+] Can't repair, last error = %d\n", GetLastError());
		return;
	}
	DWORD nr;
	BOOL bSuccess;
	bSuccess = ReadFile(m_hFile, &m_qwSegmentCount, sizeof(m_qwSegmentCount), &nr, NULL);
	bSuccess = ReadFile(m_hFile, &m_qwMaxSegmentCount, sizeof(m_qwMaxSegmentCount), &nr, NULL);
	bSuccess = ReadFile(m_hFile, &m_qwFileSize, sizeof(m_qwFileSize), &nr, NULL);

	/*
		Check for uncompleted segments
	*/
	for (DWORD64 i = 0uLL; i < m_qwSegmentCount; ++i)
	{
		sTmp = m_sDirPath + std::to_wstring(i);
		HANDLE hFile = CreateFileW(sTmp.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE) // Segment is not there ? -> Redownload
		{
			goto save;
		}
		else
		{
			DWORD dwFileSize = GetFileSize(hFile, NULL);
			if (dwFileSize != (DWORD)qwSegmentSize)
				goto save; // Segment doesn't have full size -> Redownload
			else
			{
				CloseHandle(hFile);
				continue;
			}
		}
	save:
		Range range;
		range.start = i * qwSegmentSize;
		range.ord = i;
		if (i != m_qwMaxSegmentCount - 1)
			range.end = range.start + (qwSegmentSize - 1);
		else
			range.end = range.start + (m_qwFileSize % qwSegmentSize) - 1;
		m_uncompletedSegments.push_back(range);
		if (hFile)
			CloseHandle(hFile);
	}
}

void DownloadManager::SegmentFactory::writeInfo()
{
	/*
		Write info file.
	*/
	std::wstring sTmp = m_sDirPath + INFO_FILENAME;
	m_hFile = CreateFileW(sTmp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (m_hFile == INVALID_HANDLE_VALUE)
	{
		Utils::info(L"[+] Can't create info, last error = %d\n", GetLastError());
		return;
	}
	// Write info
	// struct {
	//		DWORD64 m_qwSegmentCount;
	//		DWORD64 m_qwMaxSegmentCount;
	//		DWORD64 m_qwFileSize;
	// };
	DWORD nw;
	WriteFile(m_hFile, &m_qwSegmentCount, sizeof(m_qwSegmentCount), &nw, NULL);
	WriteFile(m_hFile, &m_qwMaxSegmentCount, sizeof(m_qwMaxSegmentCount), &nw, NULL);
	WriteFile(m_hFile, &m_qwFileSize, sizeof(m_qwFileSize), &nw, NULL);
}

void DownloadManager::SegmentFactory::updateLastestSegment(DWORD64 idx)
{
	/*
		Update to know where we stop last time.
	*/
	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		SetFilePointer(m_hFile, 0, NULL, FILE_BEGIN);
		DWORD nw;
		WriteFile(m_hFile, &idx, sizeof(idx), &nw, NULL);
	}
}

void DownloadManager::SegmentFactory::closeFile()
{
	/*
		Close file which we use to update "latest segment".
	*/
	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		Utils::info(L"[+] Closed\n");
		CloseHandle(m_hFile);
		m_hFile = NULL;
	}
}

void DownloadManager::FileMerger::start()
{
	/*
		Start merging files.
	*/
	BYTE buffer[4096];
	if (m_qwTotalSegment == 0uLL || m_qwTotalSegment == MAXDWORD64)
	{
		Utils::info(L"[+] SemgentCount invalid\n");
		return;
	}
	std::wstring sFullPath = m_sFullPath;
	HANDLE hFile = CreateFileW(sFullPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		Utils::info(L"[+] Can't create new file, last error = %d\n", GetLastError());
		return;
	}
	for (DWORD64 i = 0uLL; i < m_qwTotalSegment; ++i)
	{
		std::wstring sTmp = m_sDirPath + std::to_wstring(i);
		HANDLE hTmpFile = CreateFileW(sTmp.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hTmpFile == INVALID_HANDLE_VALUE)
		{
			Utils::info(L"[+] Missing %d, exiting ..., last error = %d\n", i, GetLastError());
			break;
		}
		DWORD dwFileSizeLow, dwFileSizeHigh;
		DWORD64 dwFileSize = 0uLL;
		dwFileSizeLow = GetFileSize(hTmpFile, &dwFileSizeHigh);
		dwFileSize = (DWORD64)dwFileSizeLow | (((DWORD64)dwFileSizeHigh) << 32);
		Utils::info(L"[+] File %lld, size = %lld\n", i, dwFileSize);
		DWORD64 n = 0;
		while (n < dwFileSize)
		{
			DWORD nr, nw;
			ReadFile(hTmpFile, buffer, sizeof(buffer), &nr, NULL); // Read from temp file
			n += nr;
			WriteFile(hFile, buffer, nr, &nw, NULL); // Write to output file.
		}
		CloseHandle(hTmpFile);
		DeleteFileW(sTmp.c_str());
	}
	CloseHandle(hFile);
}
