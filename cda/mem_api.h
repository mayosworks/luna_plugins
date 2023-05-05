//=============================================================================
// Windows/Memory API (2016/08/08版)
//                                                Copyright (c) 2006-2016 MAYO.
//=============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//-----------------------------------------------------------------------------
// API定義
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
// メモリ確保
//-----------------------------------------------------------------------------
inline void* mem_alloc(size_t size)
{
	return HeapAlloc(GetProcessHeap(), 0, size);
}

//-----------------------------------------------------------------------------
// メモリ解放
//-----------------------------------------------------------------------------
inline void mem_free(void* ptr)
{
	HeapFree(GetProcessHeap(), 0, ptr);
}

//-----------------------------------------------------------------------------
// メモリコピー
//-----------------------------------------------------------------------------
inline void mem_copy(void* dest, const void* src, size_t length)
{
	RtlMoveMemory(dest, src, length);
}

//-----------------------------------------------------------------------------
// メモリムーブ
//-----------------------------------------------------------------------------
inline void mem_move(void* dest, const void* src, size_t length)
{
	RtlMoveMemory(dest, src, length);
}

//-----------------------------------------------------------------------------
// メモリクリア
//-----------------------------------------------------------------------------
inline void mem_zero(void* ptr, size_t size)
{
	RtlZeroMemory(ptr, size);
}

//-----------------------------------------------------------------------------
// メモリフィル
//-----------------------------------------------------------------------------
inline void mem_fill(void* dest, size_t length, unsigned char value)
{
	RtlFillMemory(dest, length, value);
}

