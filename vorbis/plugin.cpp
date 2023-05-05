//=============================================================================
// Vorbisプラグイン実装
//=============================================================================

#include <windows.h>
#include "vorbis/vorbisfile.h"
#include "luna_pi.h"

//-----------------------------------------------------------------------------
// 定義
//-----------------------------------------------------------------------------
namespace {

// デコードサイズ
const UINT DECODE_SIZE = 4096;
const UINT DECODE_BITS = 16;

// 再生時コンテキスト
struct Context
{
	OggVorbis_File	ovf;
	char			data[DECODE_SIZE];	// デコード用バッファ
	int				size;				// データサイズ
	int				used;				// データ使用量
};

} //namespace

// プロトタイプ宣言
static int FileClose(void* datasource);
static size_t FileRead(void* ptr, size_t size, size_t nmemb, void* datasource);
static int FileSeek(void* datasource, ogg_int64_t offset, int whence);
static long FileTell(void* datasource);

//-----------------------------------------------------------------------------
// Dll Entry Point
//-----------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE instance, DWORD call_reason, void* /*reserved*/)
{
	if (DLL_PROCESS_ATTACH == call_reason) {
		DisableThreadLibraryCalls(instance);
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// 情報表示
//-----------------------------------------------------------------------------
static void LPAPI Property(HINSTANCE inst, HWND parent)
{
	const TCHAR* const COPYRIGHT = 
		TEXT("このプラグインは、libogg v1.3.2, libvorbis v1.3.5 を使用しています。\n\n") \
		TEXT("THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2015\n") \
		TEXT("by the Xiph.Org Foundation http://www.xiph.org/");

	MessageBox(parent, COPYRIGHT, TEXT("Property"), MB_ICONINFORMATION | MB_OK);
}

//-----------------------------------------------------------------------------
// 解析
//-----------------------------------------------------------------------------
static int LPAPI Parse(const wchar_t* path, Metadata* meta)
{
	OggVorbis_File ovf;
	ov_callbacks ovc;

	ovc.read_func  = FileRead;
	ovc.seek_func  = FileSeek;
	ovc.close_func = FileClose;
	ovc.tell_func  = FileTell;

	HANDLE file = CreateFile(path, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	if (ov_open_callbacks(file, &ovf, NULL, -1, ovc) < 0) {
		CloseHandle(file);
		return false;
	}

	meta->duration = UINT(ov_time_total(&ovf, 0) * 1000.0);
	meta->seekable = (ov_seekable(&ovf) != 0);

	vorbis_comment* vc = ov_comment(&ovf, 0);
	char* tag = NULL;

	tag = vorbis_comment_query(vc, "TITLE", 0);
	if (tag) {
		MultiByteToWideChar(CP_UTF8, 0, tag, -1, meta->title, META_MAXLEN);
	}

	tag = vorbis_comment_query(vc, "ARTIST", 0);
	if (tag) {
		MultiByteToWideChar(CP_UTF8, 0, tag, -1, meta->artist, META_MAXLEN);
	}

	tag = vorbis_comment_query(vc, "ALBUM", 0);
	if (tag) {
		MultiByteToWideChar(CP_UTF8, 0, tag, -1, meta->album, META_MAXLEN);
	}

	vorbis_info* vi = ov_info(&ovf, 0);
	wsprintf(meta->extra, L"Ogg Vorbis %dkbps", vi->bitrate_nominal / 1000);

	ov_clear(&ovf);
	return true;
}

//-----------------------------------------------------------------------------
// 再生するデータを開く
//-----------------------------------------------------------------------------
static Handle LPAPI Open(const wchar_t* path, Output* out)
{
	HANDLE file = CreateFile(path, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	Context* cxt = new Context();
	if (!cxt) {
		CloseHandle(file);
		return NULL;
	}

	cxt->size = 0;
	cxt->used = 0;

	ov_callbacks ovc;

	ovc.read_func  = FileRead;
	ovc.seek_func  = FileSeek;
	ovc.close_func = FileClose;
	ovc.tell_func  = FileTell;

	if (ov_open_callbacks(file, &cxt->ovf, NULL, -1, ovc) < 0) {
		delete cxt;
		CloseHandle(file);
		return NULL;
	}

	vorbis_info* vi = ov_info(&cxt->ovf, -1);
	if (!vi) {
		ov_clear(&cxt->ovf);
		delete cxt;
		return NULL;
	}

	out->sample_rate	= vi->rate;
	out->sample_bits	= DECODE_BITS;
	out->num_channels	= vi->channels;
	out->unit_length	= DECODE_SIZE;
	return cxt;
}

//-----------------------------------------------------------------------------
// 再生するデータを閉じる
//-----------------------------------------------------------------------------
static void LPAPI Close(Handle handle)
{
	Context* cxt = static_cast<Context*>(handle);
	if (cxt) {
		ov_clear(&cxt->ovf);
		delete cxt;
	}
}

//-----------------------------------------------------------------------------
// 読み取り
//-----------------------------------------------------------------------------
static int LPAPI Render(Handle handle, void* buffer, int size)
{
	Context* cxt = static_cast<Context*>(handle);
	if (!cxt) {
		return 0;
	}

	int used = cxt->size - cxt->used;
	if (used > 0) {
		CopyMemory(buffer, cxt->data + cxt->used, used);
		cxt->size = 0;
		cxt->used = 0;
	}

	while (used < size) {
		int bitstream = 0;
		long rsize = ov_read(&cxt->ovf, cxt->data, sizeof(cxt->data), 0, 2, 1, &bitstream);
		if (rsize <= 0) {
			return used;
		}

		int copy = rsize;
		if (used + copy > size) {
			copy = size - used;
			cxt->size = rsize;
			cxt->used = copy;
		}

		CopyMemory(static_cast<char*>(buffer) + used, cxt->data, copy);
		used += copy;
	}

	return used;
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
static int LPAPI Seek(Handle handle, int time_ms)
{
	Context* cxt = static_cast<Context*>(handle);
	if (cxt) {
		ov_time_seek(&cxt->ovf, double(time_ms) / 1000.0);
		return time_ms;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// プラグインエクスポート関数
//-----------------------------------------------------------------------------
LPEXPORT LunaPlugin* GetLunaPlugin(HINSTANCE /*instance*/)
{
	static LunaPlugin plugin;

	plugin.plugin_kind = KIND_PLUGIN;
	plugin.plugin_name = L"Ogg Vorbis plugin v1.05";
	plugin.support_type = L"*.ogg;*.oga";

	plugin.Release	= NULL;
	plugin.Property	= Property;
	plugin.Parse	= Parse;
	plugin.Open		= Open;
	plugin.Close	= Close;
	plugin.Render	= Render;
	plugin.Seek		= Seek;

	return &plugin;
}

//-----------------------------------------------------------------------------
// クローズ関数
//-----------------------------------------------------------------------------
int FileClose(void* datasource)
{
	HANDLE file = reinterpret_cast<HANDLE>(datasource);

	CloseHandle(file);
	return 0;
}

//-----------------------------------------------------------------------------
// 読み取り関数
//-----------------------------------------------------------------------------
size_t FileRead(void* ptr, size_t size, size_t nmemb, void* datasource)
{
	HANDLE file = reinterpret_cast<HANDLE>(datasource);

	DWORD readed = 0;
	if (!ReadFile(file, ptr, static_cast<DWORD>(size * nmemb), &readed, NULL)) {
		return 0;
	}

	return static_cast<size_t>(nmemb);
}

//-----------------------------------------------------------------------------
// シーク関数
//-----------------------------------------------------------------------------
int FileSeek(void* datasource, ogg_int64_t offset, int whence)
{
	HANDLE file = reinterpret_cast<HANDLE>(datasource);

	DWORD low  = static_cast<DWORD>(offset & 0x00000000FFFFFFFF);
	LONG  high = static_cast<LONG>((offset >> 32) & 0x00000000FFFFFFFF);
	if (high > 0 && offset < 0) {
		high = -high;
	}

	DWORD ret = SetFilePointer(file, low, &high, whence);
	return 0;
}

//-----------------------------------------------------------------------------
// ファイルポインタ位置取得関数
//-----------------------------------------------------------------------------
long FileTell(void* datasource)
{
	HANDLE file = reinterpret_cast<HANDLE>(datasource);

	DWORD pos = SetFilePointer(file, 0, NULL, FILE_CURRENT);
	return static_cast<long>(pos);
}
