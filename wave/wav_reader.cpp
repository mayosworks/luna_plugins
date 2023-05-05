//=============================================================================
// WAV読み取り
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include "wav_reader.h"

namespace {

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
// four_ccで指定されたチャンクへシークする
//-----------------------------------------------------------------------------
bool SeekToChunk(HANDLE file, const char* four_cc, DWORD& size)
{
	FOURCC target_cc = mmioFOURCC(four_cc[0], four_cc[1], four_cc[2], four_cc[3]);
	while (true) {
		DWORD readed = 0;

		FOURCC data_cc = ReadFourCC(file);
		ReadFile(file, &size, sizeof(size), &readed, NULL);
		if (readed != sizeof(size)) {
			return false;
		}

		if (data_cc == target_cc) {
			return true;
		}

		// サイズは、最低WORDアラインメントがいる
		if ((size & 1) == 1) {
			++size;
		}

		SetFilePointer(file, size, NULL, FILE_CURRENT);
		if (GetLastError() != NO_ERROR) {
			return false;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// .wavを開く
//-----------------------------------------------------------------------------
HANDLE OpenSource(const wchar_t* path)
{
	HANDLE file = CreateFile(path, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return INVALID_HANDLE_VALUE;
	}

	FOURCC riff_cc = ReadFourCC(file);
	if (riff_cc != mmioFOURCC('R', 'I', 'F', 'F')) {
		CloseHandle(file);
		return INVALID_HANDLE_VALUE;
	}

	DWORD readed = 0;
	DWORD riff_size = 0;
	ReadFile(file, &riff_size, sizeof(riff_size), &readed, NULL);
	// RIFFサイズ（※サイズが間違ってる場合があるので、チェックはしない）
	//if (riff_size != GetFileSize(file, NULL) - 8) {
	//	CloseHandle(file);
	//	return INVALID_HANDLE_VALUE;
	//}

	FOURCC wave_cc = ReadFourCC(file);
	if (wave_cc != mmioFOURCC('W', 'A', 'V', 'E')) {
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
	DWORD chunk_size = 0;
	if (!SeekToChunk(file, "fmt ", chunk_size)) {
		return false;
	}

	WAVEFORMATEXTENSIBLE wfex;
	RtlZeroMemory(&wfex, sizeof(wfex));

	DWORD size = chunk_size;
	if (chunk_size > sizeof(wfex)) {
		size = sizeof(wfex);
	}

	DWORD readed = 0;

	// Read format tag
	ReadFile(file, &wfex, size, &readed, NULL);
	if (size != readed) {
		return false;
	}

	// チャンクのサイズの方が大きい場合は、シークしておく
	if (size < chunk_size) {
		if ((chunk_size & 1) == 1) {
			++chunk_size;
		}

		SetFilePointer(file, chunk_size - size, NULL, FILE_CURRENT);
		if (GetLastError() != NO_ERROR) {
			return false;
		}
	}

	RtlMoveMemory(&wfx, &wfex.Format, sizeof(wfx));
	wfx.cbSize = 0;

	// WAVEFORMATの場合、wBitsPerSampleがないので、nAvgBytesPerSecから算出
	if (wfx.wBitsPerSample == 0 && wfx.nSamplesPerSec != 0 && wfx.nChannels != 0) {
		wfx.wBitsPerSample = static_cast<WORD>(wfx.nAvgBytesPerSec / (wfx.nSamplesPerSec * wfx.nChannels));
	}

	if (wfx.wFormatTag == WAVE_FORMAT_PCM /*|| wfx.wFormatTag == WAVE_FORMAT_IEEE_FLOAT*/) {
		return true;
	}

	if (wfx.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		if (InlineIsEqualGUID(wfex.SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
			wfx.wFormatTag = WAVE_FORMAT_PCM;
			return true;
		}
	
		/*if (InlineIsEqualGUID(wfex.SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
			wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
			return true;
		}*/
	}

	return false;
}

//-----------------------------------------------------------------------------
// メタデータを取得する
//-----------------------------------------------------------------------------
void GetMetadata(HANDLE file, Metadata* meta)
{
	DWORD size = 0;
	if (!SeekToChunk(file, "LIST", size)) {
		return;
	}

	char* data = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, size + 32));
	if (!data) {
		return;
	}

	DWORD readed = 0;
	ReadFile(file, data, size, &readed, NULL);
	if (readed != size) {
		HeapFree(GetProcessHeap(), 0, data);
		return;
	}

	for (DWORD i = 0; i < size;) {
		FOURCC four_cc = *reinterpret_cast<FOURCC*>(&data[i]);
		i += sizeof(four_cc);

		// sizeがないフィールドを飛ばす
		if (four_cc == mmioFOURCC('I', 'N', 'F', 'O')) {
			continue;
		}

		DWORD cc_size = *reinterpret_cast<DWORD*>(&data[i]);
		i += sizeof(cc_size);

		switch (four_cc) {
		case mmioFOURCC('I', 'N', 'A', 'M'):
			MultiByteToWideChar(CP_ACP, 0, &data[i], cc_size, meta->title, META_MAXLEN);
			break;

		case mmioFOURCC('I', 'A', 'R', 'T'):
			MultiByteToWideChar(CP_ACP, 0, &data[i], cc_size, meta->artist, META_MAXLEN);
			break;

		case mmioFOURCC('I', 'P', 'R', 'D'):
			MultiByteToWideChar(CP_ACP, 0, &data[i], cc_size, meta->album, META_MAXLEN);
			break;
		}

		i += cc_size;
		if ((cc_size & 1) == 1) {
			++i;
		}
	}

	HeapFree(GetProcessHeap(), 0, data);
}

} //namespace


//-----------------------------------------------------------------------------
// 解析
//-----------------------------------------------------------------------------
bool WavReader::Parse(const wchar_t* path, Metadata* meta)
{
	WavReader reader;
	if (!reader.Open(path)) {
		return false;
	}

	GetMetadata(reader.m_file, meta);

	meta->duration = MulDiv(reader.m_size, 1000, reader.m_format.nAvgBytesPerSec);
	meta->seekable = true;

	wsprintf(meta->extra, L"WAVE, %d Hz, %d bit, %d ch",
		reader.m_format.nSamplesPerSec, reader.m_format.wBitsPerSample, reader.m_format.nChannels);

	reader.Close();
	return true;
}

//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
WavReader::WavReader()
	: m_file(INVALID_HANDLE_VALUE)
	, m_format()
	, m_size(0)
	, m_fptr(0)
{
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
WavReader::~WavReader()
{
	Close();
}

//-----------------------------------------------------------------------------
// 開く
//-----------------------------------------------------------------------------
bool WavReader::Open(const wchar_t* path)
{
	HANDLE file = OpenSource(path);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	if (!GetPcmFormat(file, m_format)) {
		CloseHandle(file);
		return false;
	}

	DWORD size = 0;
	if (!SeekToChunk(file, "data", size)) {
		CloseHandle(file);
		return false;
	}

	m_file = file;
	m_size = size;
	m_fptr = SetFilePointer(file, 0, NULL, FILE_CURRENT);
	return true;
}

//-----------------------------------------------------------------------------
// 閉じる
//-----------------------------------------------------------------------------
void WavReader::Close()
{
	if (m_file != INVALID_HANDLE_VALUE) {
		CloseHandle(m_file);
		m_file = INVALID_HANDLE_VALUE;
	}
}

//-----------------------------------------------------------------------------
// フォーマット取得
//-----------------------------------------------------------------------------
const WAVEFORMATEX& WavReader::GetFormat() const
{
	return m_format;
}

//-----------------------------------------------------------------------------
// 読み取り
//-----------------------------------------------------------------------------
int WavReader::Read(void* buffer, int size)
{
	DWORD end = m_fptr + m_size;
	DWORD cur = SetFilePointer(m_file, 0, NULL, FILE_CURRENT);
	if (cur + size >= end) {
		size = end - cur;
	}

	DWORD readed = 0;
	if (ReadFile(m_file, buffer, size, &readed, NULL)) {
		return readed;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
int WavReader::Seek(int time_ms)
{
	DWORD fp = MulDiv(time_ms, m_format.nAvgBytesPerSec, 1000);
	fp -= (fp % m_format.nBlockAlign);

	if (SetFilePointer(m_file, m_fptr + fp, 0, FILE_BEGIN)) {
		return time_ms;
	}

	return 0;
}
