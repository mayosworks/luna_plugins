//=============================================================================
// CAF読み取り
//=============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "luna_pi.h"
#include "reader.h"

//-----------------------------------------------------------------------------
// CAF読み取り
//-----------------------------------------------------------------------------
class CafReader : public Reader
{
public:
	static bool Parse(const wchar_t* path, Metadata* meta);

public:
	CafReader();
	virtual ~CafReader();

	virtual bool Open(const wchar_t* path);
	virtual void Close();

	virtual const WAVEFORMATEX& GetFormat() const;

	virtual int Read(void* buffer, int length);
	virtual int Seek(int time_ms);

private:
	HANDLE			m_file;
	WAVEFORMATEX	m_format;
	DWORD			m_size;
	DWORD			m_fptr;
	bool			m_isle;
};
