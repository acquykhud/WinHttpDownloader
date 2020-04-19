#include "DownloadManager.h"

int main(int argc, char* argv[])
{
	// https://tb.rg-adguard.net/dl.php?go=349c795e
	DownloadManager dm(
		L"https://downloads.volatilityfoundation.org/releases/2.6/volatility_2.6_lin64_standalone.zip",
		L"C:\\Users\\xikhud\\Desktop\\Out\\LastFile.zip",
		3, // thread
		9); // conn

	DWORD64 qwSize = dm.getFileSize();

	Utils::info(L"[+] Filesize: %lld\n", qwSize);

	auto start = std::chrono::high_resolution_clock::now();
	dm.start();
	auto end = std::chrono::high_resolution_clock::now();
	auto second = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	Utils::info(L"[+] Milliseconds: %lld\n", second);
	Utils::info(L"[+] Merging...\n");
	dm.merge();
	return 0;
}	