//=============================================================================
// SND読み取り
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include "snd_reader.h"

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
// .sndを開く
//-----------------------------------------------------------------------------
HANDLE OpenSource(const wchar_t* path)
{
	HANDLE file = CreateFile(path, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return INVALID_HANDLE_VALUE;
	}

	FOURCC data_cc = ReadFourCC(file);
	if (data_cc != mmioFOURCC('.', 's', 'n', 'd')) {
		CloseHandle(file);
		return INVALID_HANDLE_VALUE;
	}

	return file;
}

//-----------------------------------------------------------------------------
// PCMフォーマットを取得する
//-----------------------------------------------------------------------------
bool GetPcmFormat(HANDLE file, WAVEFORMATEX& wfx)
{
	SetFilePointer(file, 12, NULL, FILE_BEGIN);

	WORD sample_bits = 0;

	// エンコードについては…
	// https://ja.wikipedia.org/wiki/Sun%E3%82%AA%E3%83%BC%E3%83%87%E3%82%A3%E3%82%AA%E3%83%95%E3%82%A1%E3%82%A4%E3%83%AB
	DWORD encode = ReadInt32(file);
	switch (encode) {
	case 1:  //= 8ビット G.711 μ-law:
	case 6:  //= 32ビットIEEE浮動小数点数
	case 7:  //= 64ビットIEEE浮動小数点数
	case 8:  //= 断片化されたサンプルデータ
	case 9:  //= DSPプログラム
	case 10: //= 8ビット固定小数点数
	case 11: //= 16ビット固定小数点数
	case 12: //= 24ビット固定小数点数
	case 13: //= 32ビット固定小数点数
	case 18: //= 16ビット線形（強調あり）
	case 19: //= 16ビット線形（圧縮）
	case 20: //= 16ビット線形（強調と圧縮）
	case 21: //= MusicKit DSP コマンド
	case 23: //= 4ビット ISDN μ-law圧縮。ITU-T G.721 ADPCM 音声データ符号化法を使用
	case 24: //= ITU-T G.722 ADPCM
	case 25: //= ITU-T G.723 3ビット ADPCM
	case 26: //= ITU-T G.723 5ビット ADPCM
	case 27: //= 8ビット G.711 A-law
	default:
		return false;

	case 2:	//= 8ビット線形PCM
		sample_bits = 8;
		break;

	case 3:	//= 16ビット線形PCM
		sample_bits = 16;
		break;

	case 4:	//= 24ビット線形PCM
		sample_bits = 24;
		break;

	case 5:	//= 32ビット線形PCM
		sample_bits = 32;
		break;
	}

	DWORD sample_rate = ReadInt32(file);
	WORD num_channels = static_cast<WORD>(ReadInt32(file));

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
bool SndReader::Parse(const wchar_t* path, Metadata* meta)
{
	SndReader reader;
	if (!reader.Open(path)) {
		return false;
	}

	meta->duration = MulDiv(reader.m_size, 1000, reader.m_format.nAvgBytesPerSec);
	meta->seekable = true;

	wsprintf(meta->extra, L"Sun AU, %d Hz, %d bit, %d ch",
		reader.m_format.nSamplesPerSec, reader.m_format.wBitsPerSample, reader.m_format.nChannels);

	reader.Close();
	return true;
}

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
SndReader::SndReader()
	: m_file(INVALID_HANDLE_VALUE)
	, m_format()
	, m_size(0)
	, m_fptr(0)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
SndReader::~SndReader()
{
	Close();
}

//-----------------------------------------------------------------------------
// 開く
//-----------------------------------------------------------------------------
bool SndReader::Open(const wchar_t* path)
{
	HANDLE file = OpenSource(path);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	if (!GetPcmFormat(file, m_format)) {
		CloseHandle(file);
		return false;
	}

	SetFilePointer(file, 4, NULL, FILE_BEGIN);

	m_fptr = ReadInt32(file);
	m_size = ReadInt32(file);

	if (m_size == 0xffffffff) {
		m_size = GetFileSize(file, NULL) - m_fptr;
	}

	if (m_size == 0 || m_fptr == 0) {
		CloseHandle(file);
		return false;
	}

	SetFilePointer(file, m_fptr, NULL, FILE_BEGIN);

	m_file = file;
	return true;
}

//-----------------------------------------------------------------------------
// 閉じる
//-----------------------------------------------------------------------------
void SndReader::Close()
{
	if (m_file != INVALID_HANDLE_VALUE) {
		CloseHandle(m_file);
		m_file = INVALID_HANDLE_VALUE;
	}
}

//-----------------------------------------------------------------------------
// フォーマット取得
//-----------------------------------------------------------------------------
const WAVEFORMATEX& SndReader::GetFormat() const
{
	return m_format;
}

//-----------------------------------------------------------------------------
// 読み取り
//-----------------------------------------------------------------------------
int SndReader::Read(void* buffer, int size)
{
	DWORD end = m_fptr + m_size;
	DWORD cur = SetFilePointer(m_file, 0, NULL, FILE_CURRENT);
	if (cur + size >= end) {
		size = end - cur;
	}

	DWORD readed = 0;
	if (ReadFile(m_file, buffer, size, &readed, NULL)) {
		if (m_format.wBitsPerSample == 16) {
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
int SndReader::Seek(int time_ms)
{
	DWORD fp = MulDiv(time_ms, m_format.nAvgBytesPerSec, 1000);
	fp -= (fp % m_format.nBlockAlign);

	if (SetFilePointer(m_file, m_fptr + fp, 0, FILE_BEGIN)) {
		return time_ms;
	}

	return 0;
}
