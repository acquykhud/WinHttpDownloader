#pragma once

#define SEGMENT_SIZE (1048576uLL) // 1MB

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
	class Config
	{
	public:
		DWORD dwThread;
		DWORD dwConn;
		BOOL bResume; // resuming ? or what 
		BOOL bReady;  // ready to download ?
		std::wstring sUrl;
		std::wstring sFullPath;
		std::wstring sDirPath; // Directory that contains the file to be saved.
		std::wstring sFileName; // File name (without the containing directory path)
		std::wstring sSHA256; // = sha256(sFullPath+sUrl)
		//void writeConfigToFile(const std::wstring& path);
		//void readConfigFromFile(const std::wstring& path);
	};
	class SegmentFactory
	{
	public:
		static const DWORD64 qwSegmentSize = SEGMENT_SIZE;
		SegmentFactory(DWORD64 qwFileSize, const std::wstring& sDirPath, const std::wstring& sSHA256);
		SegmentFactory(DWORD64 qwFileSize, const Config& config);
		~SegmentFactory();
		Range getNextSegment();
		bool isDataAvail() const { return m_uncompletedSegments.empty() == false || m_qwSegmentCount < m_qwMaxSegmentCount; }
		DWORD64 getMaxSegmentCount() const { return m_qwMaxSegmentCount; }

		void repair();
		void writeInfo();
		void updateLastestSegment(DWORD64 idx);

		void closeFile();

	private:
		std::mutex m_mutex;
		DWORD64 m_qwSegmentCount;
		DWORD64 m_qwMaxSegmentCount;
		DWORD64 m_qwFileSize;
		HANDLE m_hFile; // File to save information
		std::wstring m_sDirPath;
		std::wstring m_sSHA256;

		std::vector<Range> m_uncompletedSegments;
	};
	class FileMerger
	{
	public:
		FileMerger(const std::wstring& sFullPath, const std::wstring& sDirPath, const std::wstring& sFileName, DWORD64 qwMaxSegmentCount) 
			:m_sFullPath(sFullPath), m_sDirPath(sDirPath), m_sFileName(sFileName), m_qwTotalSegment(qwMaxSegmentCount) {}
		FileMerger(const Config& config, DWORD64 qwMaxSegmentCount)
			:m_sFullPath(config.sFullPath), m_sDirPath(config.sDirPath), m_qwTotalSegment(qwMaxSegmentCount) {}
		~FileMerger() {}
		void start();
	private:
		DWORD64 m_qwTotalSegment;
		std::wstring m_sFullPath;
		std::wstring m_sDirPath;
		std::wstring m_sFileName;
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
	BOOL m_bSupportResuming;
	std::vector<std::thread> m_threads;
	SegmentFactory* m_pSegmentFactory;

	void queryOptions(); // get file size and check if resuming is support;
	void downloadThread(int conn = 1); // thread function.

	void updateConfig(); // read information save by "writeConfig"
	void writeConfig(); // save information to determine whether we can resume

	void startDownloading();
	void cleanTmpFiles(); // call this in destructor, remember to close all handles

protected:
	static void __stdcall saveFile(void* lpCtx, LPBYTE lpData, DWORD nCount);
};