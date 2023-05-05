//=============================================================================
// CD Imageプラグイン実装
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <shlwapi.h>
#include "luna_pi.h"
#include "wx_misc.h"
#include "wx_text_rw.h"

//-----------------------------------------------------------------------------
// 定義
//-----------------------------------------------------------------------------
static bool IsEqual(const wchar_t* s1, const wchar_t* s2, int len);
static bool ParseCueSheet(const wchar_t* cue_path, wchar_t* img_path, int& track_num);

static const DWORD BLOCK_SIZE = 176400;

//-----------------------------------------------------------------------------
// UNICODE用文字列比較
//-----------------------------------------------------------------------------
bool IsEqual(const wchar_t* s1, const wchar_t* s2, int len)
{
	for (int i = 0; i < len; ++i) {
		if (s1[i] != s2[i]) {
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// CueSheet解析
//-----------------------------------------------------------------------------
bool ParseCueSheet(const wchar_t* cue_path, wchar_t* img_path, int& track_num)
{
	track_num = 0;
	img_path[0] = L'\0';

	wx::TextReader reader;
	if (!reader.Open(cue_path)) {
		return false;
	}

	while (reader.HasMoreData()) {
		const wchar_t* str = reader.ReadLine(true);
		int len = lstrlen(str);

		if (IsEqual(str, L"TRACK", 5)) {
			// 全トラックオーディオであること
			int pos = len - 5;
			if (!IsEqual(str + pos, L"AUDIO", 5)) {
				return false;
			}

			int no = 0;
			if (!StrToIntEx( &str[5], 0, &no)) {
				return false;
			}

			++track_num;
		}
		else if (IsEqual(str, L"FILE", 4)) {
			int pos = len - 6;
			if (!IsEqual(str + pos, L"BINARY", 6)) {
				return false;
			}

			wchar_t buf[MAX_PATH];

			lstrcpyn(buf, &str[4], pos - 4);
			StrTrim(buf, L" \"");

			lstrcpy(img_path, cue_path);
			PathRemoveFileSpec(img_path);
			PathAppend(img_path, PathFindFileName(buf));

			// イメージファイルの存在チェック
			if (!PathFileExists(img_path)) {
				return false;
			}
		}
	}

	// トラックなしか、イメージファイルなしなら、解析失敗で
	if (track_num == 0 || img_path[0] == L'\0') {
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Dll Entry Point
//-----------------------------------------------------------------------------
BOOL WINAPI DllMain( HINSTANCE instance, DWORD call_reason, void* /*reserved*/)
{
	if (DLL_PROCESS_ATTACH == call_reason) {
		DisableThreadLibraryCalls(instance);
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// 解析
//-----------------------------------------------------------------------------
static int LPAPI Parse(const wchar_t* path, Metadata* meta)
{
	wchar_t img_path[MAX_PATH];
	int track_num = 0;

	if (!ParseCueSheet(path, img_path, track_num)) {
		return false;
	}

	HANDLE file = CreateFile(img_path, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	DWORD size = GetFileSize(file, NULL);
	CloseHandle(file);

	meta->seekable = true;
	meta->duration = MulDiv(size, 1000, BLOCK_SIZE);

	wsprintf(meta->extra, L"CueSheet %d Track(s)", track_num);
	return true;
}

//-----------------------------------------------------------------------------
// 再生するデータを開く
//-----------------------------------------------------------------------------
static Handle LPAPI Open(const wchar_t* path, Output* out)
{
	wchar_t img_path[MAX_PATH];
	int track_num = 0;

	if (!ParseCueSheet(path, img_path, track_num)) {
		return NULL;
	}

	HANDLE file = CreateFile(img_path, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	out->sample_rate	= 44100;
	out->sample_bits	= 16;
	out->num_channels	= 2;
	out->unit_length	= 0;

	return file;
}

//-----------------------------------------------------------------------------
// 再生するデータを閉じる
//-----------------------------------------------------------------------------
static void LPAPI Close(Handle handle)
{
	HANDLE file = static_cast<HANDLE>(handle);
	if (file && file != INVALID_HANDLE_VALUE) {
		CloseHandle(file);
	}
}

//-----------------------------------------------------------------------------
// 読み取り
//-----------------------------------------------------------------------------
static int LPAPI Render(Handle handle, void* buffer, int size)
{
	HANDLE file = static_cast<HANDLE>(handle);
	if (file && file != INVALID_HANDLE_VALUE) {
		DWORD readed = 0;
		if (ReadFile(file, buffer, size, &readed, NULL)) {
			return static_cast<int>(readed);
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
static int LPAPI Seek(Handle handle, int time_ms)
{
	HANDLE file = static_cast<HANDLE>(handle);
	if (file && file != INVALID_HANDLE_VALUE) {
		DWORD addr = MulDiv(time_ms, BLOCK_SIZE, 1000);
		if (SetFilePointer(file, addr, NULL, FILE_BEGIN)) {
			return MulDiv(addr, 1000, BLOCK_SIZE);
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// プラグインエクスポート関数
//-----------------------------------------------------------------------------
LPEXPORT LunaPlugin* LPAPI GetLunaPlugin(HINSTANCE /*instance*/)
{
	static LunaPlugin plugin;

	plugin.plugin_kind = KIND_PLUGIN;
	plugin.plugin_name = L"CueSheet plugin v1.01";
	plugin.support_type = L"*.cue";

	plugin.Release	= NULL;
	plugin.Property	= NULL;
	plugin.Parse	= Parse;
	plugin.Open		= Open;
	plugin.Close	= Close;
	plugin.Render	= Render;
	plugin.Seek		= Seek;

	return &plugin;
}
