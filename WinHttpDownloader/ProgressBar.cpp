#include "ProgressBar.h"

#pragma region ProgressBar
void ProgressBar::setTotal(DWORD64 qwTotal)
{
	m_qwTotal = qwTotal;
}
void ProgressBar::update(DWORD64 qwCurrent)
{
	/*
		Taken and modified: https://www.codeproject.com/Tips/537904/Console-simple-progress
	*/
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_qwTotal == 0uLL)
		return;

	const int maxWidth = 72;
	const WCHAR szLabel[] = L"[+] Progress ";
	//minus label len
	int width = maxWidth - lstrlenW(szLabel);
	DWORD64 pos = (qwCurrent * maxWidth) / m_qwTotal;
	DWORD64 percent = (qwCurrent * 100) / m_qwTotal;

	if (percent != 100)
	{
		Utils::info(L"%s[", szLabel);
		for (int i = 0; i < pos; i++) 
			Utils::info(L"%c", L'=');
		Utils::info(L"% *c", (DWORD)(maxWidth - pos + 1), L']');
		Utils::info(L" %3lld%%\r", percent);
	}
	else
	{
		/*
			Clear line
		*/
		for (int i = 0; i < 100; i++)
			Utils::info(L"%c", L' ');
		Utils::info(L"%c", L'\r');
	}
}
#pragma endregion