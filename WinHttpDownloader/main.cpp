#include "ArgParse.h"
#include "DownloadManager.h"

void start(const std::wstring& sUrl, const std::wstring& sPathOut, DWORD dwThread, DWORD dwConn)
{
	// https://tb.rg-adguard.net/dl.php?go=349c795e
	DownloadManager dm(sUrl, sPathOut, dwThread, dwConn);
	DWORD64 qwSize = dm.getFileSize();

	Utils::info(L"[+] Filesize: %lld bytes\n", qwSize);
	
	auto start = std::chrono::high_resolution_clock::now();
	dm.start();
	auto end = std::chrono::high_resolution_clock::now();
	auto second = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
	Utils::info(L"[+] Download time: %lld seconds\n", second);
	Utils::info(L"[+] Merging...\n");
	dm.merge();
}

int wmain(int argc, const wchar_t* argv[])
{
	argparse::ArgumentParser parser(L"Argument parser example");

	parser.add_argument()
		.names({ L"-t", L"--thread" })
		.description(L"number of thread (default 1)")
		.required(false);
	parser.add_argument()
		.names({ L"-c", L"--conn" })
		.description(L"number of connection (default 1)")
		.required(false);
	parser.add_argument()
		.names({ L"-u", L"--url" })
		.description(L"URL")
		.required(true);
	parser.add_argument()
		.names({ L"-o", L"--out"})
		.description(L"Path to save file")
		.required(true);

	parser.enable_help();
	auto err = parser.parse(argc, argv);
	if (err)
	{
		Utils::info(L"[+] Error parsing arguments");
		return -1;
	}
	if (parser.exists(L"help")) {
		parser.print_help();
		return 0;
	}

	DWORD dwThread = 1;
	if (parser.exists(L"thread"))
	{
		dwThread = parser.get<DWORD>(L"thread");
	}
	DWORD dwConn = 1;
	if (parser.exists(L"conn"))
	{
		dwConn = parser.get<DWORD>(L"conn");
	}
	std::wstring sUrl = parser.get<std::wstring>(L"url");
	std::wstring sPathOut = parser.get<std::wstring>(L"out");

	Utils::info(L"--------------------------------------------\n");
	Utils::info(L"[+] URL: %s\n", sUrl.c_str());
	Utils::info(L"[+] Out: %s\n", sPathOut.c_str());
	Utils::info(L"[+] Thread: %d, Conn: %d\n", dwThread, dwConn);
	Utils::info(L"--------------------------------------------\n");

	start(sUrl, sPathOut, dwThread, dwConn);

	return 0;
}	