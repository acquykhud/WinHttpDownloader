#include "Utils.h"

std::wstring Utils::hex_a2b(BYTE v)
{
	BYTE nipHigh = v >> 4;
	BYTE nipLow = v & 15;
	std::wstring sRet = L"";
	if (nipHigh < 10)
		sRet += nipHigh + L'0';
	else
		sRet += (nipHigh - 10) + L'A';
	if (nipLow < 10)
		sRet += nipLow + L'0';
	else
		sRet += (nipLow - 10) + L'A';
	return sRet;
}

std::wstring Utils::sha256_hexdigest(LPCBYTE lpData, DWORD dwLength)
{
	BYTE buf[SHA256_BLOCK_SIZE];
	std::wstring sRet = L"";
	SHA256_CTX ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, (const BYTE*)"abc", 3);
	sha256_final(&ctx, buf);
	for (int i = 0; i < sizeof(buf); ++i)
		sRet += hex_a2b(buf[i]);
	return sRet;
}

std::string Utils::utf8_encode(const std::wstring & wstr)
{
	/*
		Taken from https://stackoverflow.com/questions/215963/how-do-you-properly-use-widechartomultibyte
	*/
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

void Utils::info(LPCWSTR fmt, ...)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	va_list va;
	va_start(va, fmt);
	vwprintf(fmt, va);
	va_end(va);
}

std::wstring Utils::getTempPath()
{
	static std::wstring sRet(MAX_PATH, L'\u0000');
	static BOOL init = FALSE;
	if (!init)
	{
		DWORD nRead = GetTempPathW(MAX_PATH, &sRet[0]);
		sRet.resize(nRead);
		init = TRUE;
	}
	return sRet;
}
