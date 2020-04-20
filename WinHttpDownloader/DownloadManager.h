#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#define SEGMENT_SIZE (1048576uLL * 4uLL) // 1MB

#include "AsynchronousWinHttp.h"
#include "ProgressBar.h"
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
		DWORD dwThread; // number of threads
		DWORD dwConn; // number of connections
		BOOL bResume; // resuming ? or what 
		BOOL bReady;  // ready to download ?
		std::wstring sUrl; // where to download file
		std::wstring sFullPath; // where to save file
		std::wstring sDirPath; // Directory that contains the file to be saved.
		std::wstring sFileName; // File name (without the containing directory path)
		std::wstring sSHA256; // = sha256(sFullPath+sUrl)
	};
	class SegmentFactory
	{
	public:
		static const DWORD64 qwSegmentSize = SEGMENT_SIZE;
		SegmentFactory(DWORD64 qwFileSize, const Config& config);
		~SegmentFactory();

		Range getNextSegment();
		bool isDataAvail() const { return m_uncompletedSegments.empty() == false || m_qwSegmentCount < m_qwMaxSegmentCount; }
		DWORD64 getMaxSegmentCount() const { return m_qwMaxSegmentCount; }

		DWORD64 repair();    // read info written by "writeInfo"
		void writeInfo(); // write Info to know where to continue downloading

		void updateLastestSegment(DWORD64 idx); // update the lastest segment being requested to file
		

	private:
		std::mutex m_mutex;
		DWORD64 m_qwSegmentCount; // count variable
		DWORD64 m_qwMaxSegmentCount; // File size / segment size (+1)
		DWORD64 m_qwFileSize; // File size
		HANDLE m_hFile; // File to save information
		std::wstring m_sDirPath;
		std::wstring m_sSHA256;
		std::vector<Range> m_uncompletedSegments; // segments that is corrupt will be redownloaded

		void closeFile(); // close file
	};
	class FileMerger
	{
	public:
		static const DWORD64 qwSegmentSize = SEGMENT_SIZE;
		FileMerger(const Config& config, DWORD64 qwMaxSegmentCount, DWORD64 qwTotalFileSize)
			:m_sFullPath(config.sFullPath), m_sDirPath(config.sDirPath), m_qwTotalSegment(qwMaxSegmentCount), m_qwTotalFileSize(qwTotalFileSize) {}
		~FileMerger() {}
		void start(); // validate and merge
	private:
		DWORD64 m_qwTotalSegment; // how many segments we need to merge ?
		DWORD64 m_qwTotalFileSize;
		std::wstring m_sFullPath;
		std::wstring m_sDirPath;
		std::wstring m_sFileName;
		bool validate() const;
		void merge();
	};
	struct DownloadManagerCallbackContext
	{
		HANDLE hFile; // File to write
		void* this_ptr; // ptr to DownloadManager object
	};
public:
	DownloadManager(const std::wstring& sUrl, const std::wstring& sFullPath, DWORD nThread = 1, DWORD nConn = 1);
	~DownloadManager();

	DWORD64 getFileSize() const;
	BOOL supportResuming() const;
	void start();
	void merge();

private:
	Config m_conFig;
	DWORD64 m_qwRemoteFileSize;
	DWORD64 m_qwDownloadedSize; // how much we have downloaded
	BOOL m_bSupportResuming;
	std::vector<std::thread> m_threads;
	SegmentFactory* m_pSegmentFactory;
	ProgressBar m_progressBar;

	void queryOptions(); // get file size and check if resuming is support;
	void downloadThread(int conn = 1); // thread function.

	void updateConfig(); // read information save by "writeConfig"
	void writeConfig(); // save information to determine whether we can resume

	void startMultithreadedDownloadMode(); // start downloading
	void startSinglethreadedDownloadMode(); 
	void cleanTmpFiles(); // call this in destructor, remember to close all handles

protected:
	static void __stdcall CallBack(void* lpCtx, LPBYTE lpData, DWORD nCount); // call back process data.
};

#endif