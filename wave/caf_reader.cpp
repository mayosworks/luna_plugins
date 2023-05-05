//=============================================================================
// CAF読み取り
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <algorithm>
#include "caf_reader.h"

namespace {

//-----------------------------------------------------------------------------
// ビットスワップ
//-----------------------------------------------------------------------------
unsigned short Swap16(unsigned short value)
{
	return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

unsigned int Swap32(unsigned int value)
{
	return ((value & 0xFF) << 24) | (((value >> 8) & 0xFF) << 16) | (((value >> 16) & 0xFF) << 8) | ((value >> 24) & 0xFF);
}

//-----------------------------------------------------------------------------
// Float64をInt32に変換する
//-----------------------------------------------------------------------------
static int Unpack64(int exp, int fract)
{
	fract = 0x10000 | (fract >> 4) | ((exp & 0x000F) << 12);
	exp = (exp >> 4);

	int e = exp - 1023;
	if (e < 0) {
		return -1;
	}
	else if (e == 16) {
		return fract;
	}
	else if (e < 16) {
		return fract >> (16 - e);
	}
	else {
		return fract << (e - 16);
	}

	return -1;
}

//-----------------------------------------------------------------------------
// intに入れたfloatをInt24に変換する
//-----------------------------------------------------------------------------
/*static int Unpack24(INT64 x)
{
	//const int INT24_MAX_VALUE = (1 << 23) - 1;

	int e = (x >> 52) & 0xFF;
	int value = ((x & 0x7FFFFFull) + (1ull << 52)) >> (1023ull - e);
	return (x < 0)? -value : value;
}*/

//-----------------------------------------------------------------------------
// FourCCを読み込む
//-----------------------------------------------------------------------------
FOURCC ReadFourCC(HANDLE file)
{
	FOURCC value = 0;
	DWORD readed = 0;

	if (!ReadFile(file, &value, sizeof(value), &readed, NULL)) {
		return 0;
	}

	return value;
}

//-----------------------------------------------------------------------------
// 整数２バイト読み込む
//-----------------------------------------------------------------------------
WORD ReadInt16(HANDLE file)
{
	WORD value = 0;
	DWORD readed = 0;

	if (!ReadFile(file, &value, sizeof(value), &readed, NULL)) {
		return 0;
	}

	return Swap16(value);
}

//-----------------------------------------------------------------------------
// 整数４バイト読み込む
//-----------------------------------------------------------------------------
DWORD ReadInt32(HANDLE file)
{
	DWORD value = 0;
	DWORD readed = 0;

	if (!ReadFile(file, &value, sizeof(value), &readed, NULL)) {
		return 0;
	}

	return Swap32(value);
}

//-----------------------------------------------------------------------------
// four_ccで指定されたチャンクへシークする
//-----------------------------------------------------------------------------
bool SeekToChunk(HANDLE file, const char* four_cc, DWORD& size)
{
	FOURCC target_cc = mmioFOURCC(four_cc[0], four_cc[1], four_cc[2], four_cc[3]);
	while (true) {
		FOURCC data_cc = ReadFourCC(file);
		DWORD size_hi = ReadInt32(file);
		DWORD size_lo = ReadInt32(file);

		size = size_lo;

		// サイズは、最低WORDアラインメントがいる
		if ((size & 1) == 1) {
			++size;
		}

		if (data_cc == target_cc) {
			return true;
		}

		SetFilePointer(file, size, NULL, FILE_CURRENT);
		if (GetLastError() != NO_ERROR) {
			return false;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// .cafを開く
//-----------------------------------------------------------------------------
HANDLE OpenSource(const wchar_t* path)
{
	HANDLE file = CreateFile(path, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return INVALID_HANDLE_VALUE;
	}

	FOURCC form_cc = ReadFourCC(file);
	if (form_cc != mmioFOURCC('c', 'a', 'f', 'f')) {
		CloseHandle(file);
		return INVALID_HANDLE_VALUE;
	}

	DWORD mFileVersion = ReadInt16(file);	// must be 1
	DWORD mFileFlags = ReadInt16(file);		// must be 0

	return file;
}

//-----------------------------------------------------------------------------
// PCMフォーマットを取得する
//-----------------------------------------------------------------------------
bool GetPcmFormat(HANDLE file, WAVEFORMATEX& wfx, bool& is_little_endian)
{
	DWORD readed = 0;
	FOURCC data_cc = 0;

	DWORD size = 0;
	if (!SeekToChunk(file, "desc", size)) {
		return false;
	}

	// AIFFのCOMMチャンクは、32バイト以上あるはず
	if (size < 32) {
		return false;
	}

	WORD exp = ReadInt16(file);
	WORD fract = ReadInt16(file);
	WORD dummy = ReadInt16(file);
	(void)ReadInt16(file);
	DWORD sample_rate = Unpack64(exp, fract);

	FOURCC format_id = ReadFourCC(file);
	DWORD format_flag = ReadInt32(file);
	DWORD bytes_per_packet = ReadInt32(file);
	DWORD frames_per_packet = ReadInt32(file);
	WORD num_channels = static_cast<WORD>(ReadInt32(file));
	WORD sample_bits = static_cast<WORD>(ReadInt32(file));

	// 8k/11.25kの倍数以外は対応しない
	if (0 < (sample_rate % 8000) && 0 < (sample_rate % 11025)) {
		return false;
	}

	// フォーマット的には、LinerPCMのみ対応
	if (format_id != mmioFOURCC('l', 'p', 'c', 'm')) {
		return false;
	}

	//kAudioFormatLinearPCMに対するフラグ
	enum
	{
		kCAFLinearPCMFormatFlagIsFloat         = (1L << 0),
		kCAFLinearPCMFormatFlagIsLittleEndian  = (1L << 1)
	};

	// floatには対応しない
	if (format_flag & kCAFLinearPCMFormatFlagIsFloat) {
		return false;
	}

	if (format_flag & kCAFLinearPCMFormatFlagIsLittleEndian) {
		is_little_endian = true;
	}

	wfx.wFormatTag		= WAVE_FORMAT_PCM;
	wfx.nChannels		= num_channels;
	wfx.nSamplesPerSec	= sample_rate;
	wfx.wBitsPerSample	= sample_bits;
	wfx.nBlockAlign		= wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec	= wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.cbSize			= 0;

	return true;
}

} //namespace


//-----------------------------------------------------------------------------
// 解析
//-----------------------------------------------------------------------------
bool CafReader::Parse(const wchar_t* path, Metadata* meta)
{
	CafReader reader;
	if (!reader.Open(path)) {
		return false;
	}

	meta->duration = MulDiv(reader.m_size, 1000, reader.m_format.nAvgBytesPerSec);
	meta->seekable = true;

	wsprintf(meta->extra, L"CAF, %d Hz, %d bit, %d ch",
		reader.m_format.nSamplesPerSec, reader.m_format.wBitsPerSample, reader.m_format.nChannels);

	reader.Close();
	return true;
}

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
CafReader::CafReader()
	: m_file(INVALID_HANDLE_VALUE)
	, m_format()
	, m_size(0)
	, m_fptr(0)
	, m_isle(false)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
CafReader::~CafReader()
{
	Close();
}

//-----------------------------------------------------------------------------
// 開く
//-----------------------------------------------------------------------------
bool CafReader::Open(const wchar_t* path)
{
	HANDLE file = OpenSource(path);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	bool is_little_endian = false;
	if (!GetPcmFormat(file, m_format, is_little_endian)) {
		CloseHandle(file);
		return false;
	}

	DWORD size = 0;
	if (!SeekToChunk(file, "data", size)) {
		CloseHandle(file);
		return false;
	}

	DWORD edit_count = ReadInt32(file);

	m_file = file;
	m_size = size - 4;
	m_fptr = SetFilePointer(file, 0, NULL, FILE_CURRENT) + 4;
	m_isle = is_little_endian;
	return true;
}

//-----------------------------------------------------------------------------
// 閉じる
//-----------------------------------------------------------------------------
void CafReader::Close()
{
	if (m_file != INVALID_HANDLE_VALUE) {
		CloseHandle(m_file);
		m_file = INVALID_HANDLE_VALUE;
	}
}

//-----------------------------------------------------------------------------
// フォーマット取得
//-----------------------------------------------------------------------------
const WAVEFORMATEX& CafReader::GetFormat() const
{
	return m_format;
}

//-----------------------------------------------------------------------------
// 読み取り
//-----------------------------------------------------------------------------
int CafReader::Read(void* buffer, int size)
{
	DWORD end = m_fptr + m_size;
	DWORD cur = SetFilePointer(m_file, 0, NULL, FILE_CURRENT);
	if (cur + size >= end) {
		size = end - cur;
	}

	DWORD readed = 0;
	if (ReadFile(m_file, buffer, size, &readed, NULL)) {
		if (m_isle) {
			// none
		}
		else if (m_format.wBitsPerSample == 16) {
			unsigned short* data = static_cast<unsigned short*>(buffer);
			for (int i = 0; i < size / 2; ++i) {
				data[i] = Swap16(data[i]);
			}
		}
		else if (m_format.wBitsPerSample == 24) {
			unsigned char* data = static_cast<unsigned char*>(buffer);
			for (int i = 0; i < size; i += 3) {
				unsigned char temp = data[i];
				data[i] = data[i+2];
				data[i+2] = temp;
			}
		}
		else if (m_format.wBitsPerSample == 32) {
			unsigned int* data = static_cast<unsigned int*>(buffer);
			for (int i = 0; i < size / 4; ++i) {
				data[i] = Swap32(data[i]);
			}
		}

		return readed;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
int CafReader::Seek(int time_ms)
{
	DWORD fp = MulDiv(time_ms, m_format.nAvgBytesPerSec, 1000);
	fp -= (fp % m_format.nBlockAlign);

	if (SetFilePointer(m_file, m_fptr + fp, 0, FILE_BEGIN)) {
		return time_ms;
	}

	return 0;
}
