#include "DownloadManager.h"
#include <iostream>

int main(int argc, char* argv[])
{

	DownloadManager dm(L"http://vietup.net/tai-tap-tin/secret-zip/220854",
		3, // thread
		2); // conn 

	DWORD64 size = dm.getFileSize();
	BOOL sup = dm.supportResuming();

	wprintf(L"[+] Filesize: %lld\n", size);

	dm.start();



	return 0;
}	