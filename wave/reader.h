//=============================================================================
// 読み取りクラス定義
//=============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

//-----------------------------------------------------------------------------
// NTAPIを用いるための定義
//-----------------------------------------------------------------------------
#undef RtlZeroMemory
#undef RtlFillMemory
#undef RtlMoveMemory

#undef ZeroMemory
#undef FillMemory
#undef MoveMemory
#undef CopyMemory

extern "C" NTSYSAPI void NTAPI RtlZeroMemory(void* dest, size_t length);
extern "C" NTSYSAPI void NTAPI RtlFillMemory(void* dest, size_t length, BYTE pattern);
extern "C" NTSYSAPI void NTAPI RtlMoveMemory(void* dest, const void* src, size_t length);

//-----------------------------------------------------------------------------
// 既存APIの再定義
//-----------------------------------------------------------------------------
#define ZeroMemory RtlZeroMemory
#define FillMemory RtlFillMemory
#define MoveMemory RtlMoveMemory
#define CopyMemory RtlMoveMemory

//-----------------------------------------------------------------------------
// PCM読み取りベースクラス
//-----------------------------------------------------------------------------
class Reader
{
public:
	Reader() {}
	virtual ~Reader() {}

	virtual bool Open(const wchar_t* path) = 0;
	virtual void Close() = 0;

	virtual const WAVEFORMATEX& GetFormat() const = 0;

	virtual int Read(void* buffer, int length) = 0;
	virtual int Seek(int time_ms) = 0;
};
