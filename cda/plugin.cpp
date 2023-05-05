//=============================================================================
// CDDAプラグイン実装
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <shlwapi.h>
#include "mem_api.h"
#include "luna_pi.h"
#include "cd_ctrl.h"

//-----------------------------------------------------------------------------
// 定義
//-----------------------------------------------------------------------------

// 再生時コンテキスト
struct Context
{
	CDCtrl	cd_ctrl;	// CD制御
	UINT	std_sec;	// 開始セクタ
	UINT	end_sec;	// 終了セクタ
	UINT	cur_sec;	// 現在のセクタ
	BYTE*	tmp_buf;	// データ読み取りバッファ
	bool	exists;		// バッファにデータがあるか？
};

// 再生中かどうか？（２重処理防止のために使う）
static bool g_playing = false;

// operator new/delete
void* operator new(size_t size)
{
	return mem_alloc(size);
}

void operator delete(void* ptr)
{
	mem_free(ptr);
}

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
// 解析
//-----------------------------------------------------------------------------
static int LPAPI Parse(const wchar_t* path, Metadata* meta)
{
	if (lstrlen(path) < 14) {
		return false;
	}

	meta->duration = 0;
	meta->seekable = true;
	lstrcpy(meta->extra, L"Audio CD");

	if (g_playing) {
		return true;
	}

	CDCtrl cd_ctrl;
	if (!cd_ctrl.OpenDevice(path)) {
		return false;
	}

	if (!cd_ctrl.IsMediaLoaded()) {
		cd_ctrl.CloseDevice();
		return false;
	}

	TOC toc;
	if (!cd_ctrl.ReadTOC(toc)) {
		cd_ctrl.CloseDevice();
		return false;
	}

	// トラックが範囲内かどうかを調べる
	int track = (path[8] - L'0') * 10 + (path[9] - L'0') - 1;
	if (track < 0 || track >= toc.end_track_no) {
		cd_ctrl.CloseDevice();
		return false;
	}

	// データトラックなら再生しない
	if (toc.track_list[track].track_type & 0x04) {
		cd_ctrl.CloseDevice();
		return false;
	}

	UINT std_sect = toc.track_list[track    ].std_sector;
	UINT end_sect = toc.track_list[track + 1].std_sector - 1;

	meta->duration = MulDiv(end_sect - std_sect + 1, CDDA_SECT_SIZE * 1000, 176400);

	cd_ctrl.CloseDevice();
	return true;
}

//-----------------------------------------------------------------------------
// メディアを開く
//-----------------------------------------------------------------------------
static Handle LPAPI Open(const wchar_t* path, Output* out)
{
	if (lstrlen(path) < 14) {
		return NULL;
	}

	Context* cxt = new Context();
	if (!cxt) {
		return NULL;
	}

	if (!cxt->cd_ctrl.OpenDevice(path)) {
		delete cxt;
		return NULL;
	}

	if (!cxt->cd_ctrl.IsMediaLoaded()) {
		cxt->cd_ctrl.CloseDevice();
		delete cxt;
		return NULL;
	}

	TOC toc;
	if (!cxt->cd_ctrl.ReadTOC(toc)) {
		cxt->cd_ctrl.CloseDevice();
		delete cxt;
		return NULL;
	}

	// トラックが範囲内かどうかを調べる
	int track = (path[8] - L'0') * 10 + (path[9] - L'0') - 1;
	if (track < 0 || track >= toc.end_track_no) {
		cxt->cd_ctrl.CloseDevice();
		delete cxt;
		return NULL;
	}

	// データトラックなら再生しない
	if (toc.track_list[track].track_type & 0x04) {
		cxt->cd_ctrl.CloseDevice();
		delete cxt;
		return NULL;
	}

	if (!cxt->cd_ctrl.LockMedia(true)) {
		cxt->cd_ctrl.CloseDevice();
		delete cxt;
		return NULL;
	}

	if (!cxt->cd_ctrl.InitCDDA(1)) {
		cxt->cd_ctrl.LockMedia(false);
		cxt->cd_ctrl.CloseDevice();
		delete cxt;
		return NULL;
	}

	cxt->std_sec = toc.track_list[track    ].std_sector;
	cxt->end_sec = toc.track_list[track + 1].std_sector - 1;
	cxt->cur_sec = cxt->std_sec;

	out->sample_rate	= 44100;
	out->sample_bits	= 16;
	out->num_channels	= 2;
	out->unit_length	= CDDA_SECT_SIZE;

	g_playing = true;
	return cxt;
}

//-----------------------------------------------------------------------------
// メディアを閉じる
//-----------------------------------------------------------------------------
static void LPAPI Close(Handle handle)
{
	Context* cxt = static_cast<Context*>(handle);
	if (cxt) {
		cxt->cd_ctrl.TermCDDA();
		cxt->cd_ctrl.LockMedia(false);
		cxt->cd_ctrl.CloseDevice();
		delete cxt;
	}

	g_playing = false;
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

	if (cxt->cur_sec > cxt->end_sec) {
		return 0;
	}

	if (!cxt->cd_ctrl.ReadCDDA(cxt->cur_sec, 1, buffer, size)) {
		return 0;
	}

	++cxt->cur_sec;
	return size;
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
static int LPAPI Seek(Handle handle, int time_ms)
{
	Context* cxt = static_cast<Context*>( handle );
	if (!cxt) {
		return 0;
	}

	cxt->cur_sec = cxt->std_sec + MulDiv(time_ms, 176400, CDDA_SECT_SIZE * 1000);
	return MulDiv(cxt->cur_sec - cxt->std_sec, CDDA_SECT_SIZE * 1000, 176400);
}


//-----------------------------------------------------------------------------
// プラグインエクスポート関数
//-----------------------------------------------------------------------------
LPEXPORT LunaPlugin* LPAPI GetLunaPlugin(HINSTANCE /*instance*/)
{
	static LunaPlugin plugin;

	plugin.plugin_kind = KIND_PLUGIN;
	plugin.plugin_name = L"Audio CD plugin v1.03";
	plugin.support_type = L"*.cda";

	plugin.Release	= NULL;
	plugin.Property	= NULL;
	plugin.Parse	= Parse;
	plugin.Open		= Open;
	plugin.Close	= Close;
	plugin.Render	= Render;
	plugin.Seek		= Seek;
	return &plugin;
}
