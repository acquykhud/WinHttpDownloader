#pragma once
#include "AsynchronousWinHttp.h"
#include <thread>
#include <vector>
struct Range
{
	DWORD64 start;
	DWORD64 end;
	DWORD64 ord;
};

class DownloadManager
{
private:
	struct Config
	{
		std::wstring sUrl;
		DWORD dwThread;
		DWORD dwConn;
		BOOL bResume;
		std::wstring sFullPath;
		std::wstring sDir; // Directory that contains the file to be saved.
	};
	class SegmentFactory
	{
	public:
		static const DWORD64 qwSegmentSize = 100 * 1024; //(16777216uLL); // 16 MB
		SegmentFactory(DWORD64 qwFileSize);
		~SegmentFactory() {}
		Range getNextSegment();
		bool isDataAvail() const { return m_qwSegmentCount < m_qwMaxSegmentCount; }
	private:
		std::mutex m_mutex;
		DWORD64 m_qwSegmentCount;
		DWORD64 m_qwMaxSegmentCount;
		DWORD64 m_qwFileSize;
	};
public:
	DownloadManager(const std::wstring& url, DWORD nThread = 1, DWORD nConn = 1, BOOL resume = FALSE);
	~DownloadManager();

	DWORD64 getFileSize() const;
	BOOL supportResuming() const;
	void start();

private:
	Config m_conFig;
	DWORD64 m_qwRemoteFileSize;
	BOOL m_bSupportResuming;
	std::vector<std::thread> m_threads;
	SegmentFactory* m_pSegmentFactory;

	void queryOptions(); // get file size and test if resuming is support;
	void downloadThread(int conn = 1);

protected:
	static void __stdcall saveFile(void* lpCtx, LPBYTE lpData, DWORD nCount);
};