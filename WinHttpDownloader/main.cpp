#include "DownloadManager.h"

int main(int argc, char* argv[])
{
	// https://tb.rg-adguard.net/dl.php?go=349c795e
	DownloadManager dm(
		L"http://ipv4.download.thinkbroadband.com/50MB.zip",
		L"C:\\Users\\xikhud\\Desktop\\Out\\LastFile.zip",
		3, // thread
		6); // conn

	DWORD64 qwSize = dm.getFileSize();

	Utils::info(L"[+] Filesize: %lld\n", qwSize);

	auto start = std::chrono::high_resolution_clock::now();
	dm.start();
	auto end = std::chrono::high_resolution_clock::now();
	auto second = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
	Utils::info(L"[+] Download time: %lld seconds\n", second);
	Utils::info(L"[+] Merging...\n");
	dm.merge();
	return 0;
}	