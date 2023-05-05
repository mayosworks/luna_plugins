//=============================================================================
// SND読み取り
//=============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "luna_pi.h"
#include "reader.h"

//-----------------------------------------------------------------------------
// SND読み取り
//-----------------------------------------------------------------------------
class SndReader : public Reader
{
public:
	static bool Parse(const wchar_t* path, Metadata* meta);

public:
	SndReader();
	virtual ~SndReader();

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
};
