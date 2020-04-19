#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <Windows.h>
#include <mutex>
#include "Sha256.h"

static std::mutex g_mutex;

namespace Utils
{
	std::wstring hex_a2b(BYTE v);
	std::wstring sha256_hexdigest(LPCBYTE lpData, DWORD dwLength);
	std::string utf8_encode(const std::wstring &wstr);
	void info(LPCWSTR fmt, ...);
	std::wstring getTempPath();
}

#endif