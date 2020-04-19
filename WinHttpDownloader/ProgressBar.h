#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include "Utils.h"

class ProgressBar
{
public:
	ProgressBar() {};
	ProgressBar(DWORD64 qwTotal) :m_qwTotal(qwTotal) {}
	~ProgressBar() {};
	void setTotal(DWORD64 qwTotal);
	void update(DWORD64 qwCurrent);
private:
	DWORD64 m_qwTotal;
	std::mutex m_mutex;
};

#endif