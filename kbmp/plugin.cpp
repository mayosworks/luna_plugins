//=============================================================================
// KBMPプラグイン実装
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include "kmp_pi.h"
#include "luna_pi.h"

// ランタイムライブラリを使用しないための宣言
#ifdef RtlZeroMemory
#undef RtlZeroMemory
extern "C" NTSYSAPI void NTAPI RtlZeroMemory(void* dest, size_t len);
#endif //RtlZeroMemory

//-----------------------------------------------------------------------------
// プラグイン内変数
//-----------------------------------------------------------------------------
namespace {

HINSTANCE		g_dll_inst = NULL;	// このDLLのインスタンス
HMODULE			g_kpi_inst = NULL;	// プラグインのdllインスタンス
KMPMODULE*		g_kmp_mdl  = NULL;	// プラグインのインスタンス
pfnKmpConfig	g_kmp_cfg  = NULL;	// プラグイン側の設定関数
bool			g_has_cfg  = false;	// 設定用ini/関数があるかどうか？

} //namespace

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
// プラグインの読み込み
//-----------------------------------------------------------------------------
static bool LoadKPI(wchar_t* name, wchar_t* type)
{
	wchar_t kpi_path[MAX_PATH];

	GetModuleFileName(g_dll_inst, kpi_path, MAX_PATH);
	PathRenameExtension(kpi_path, L".kpi");

	if (!PathFileExists(kpi_path)) {
		return false;
	}

	// 依存DLLが見つからない等のエラーを表示させずに呼び出し元に通知させる
	UINT err_mode = SetErrorMode(SEM_FAILCRITICALERRORS);

	HMODULE kpi_inst = LoadLibraryEx(kpi_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

	// エラー非表示設定を戻す
	SetErrorMode(err_mode);

	if (!kpi_inst) {
		return false;
	}

	// プラグインのエクスポートAPIを取得
	pfnGetKMPModule GetKmpMdl = reinterpret_cast<pfnGetKMPModule>(
							GetProcAddress(kpi_inst, SZ_KMP_GETMODULE));
	if (!GetKmpMdl) {
		FreeLibrary(kpi_inst);
		return false;
	}

	KMPMODULE* kmp_mdl = GetKmpMdl();
	if (!kmp_mdl) {
		FreeLibrary(kpi_inst);
		return false;
	}

	if (kmp_mdl->Init) {
		kmp_mdl->Init();
	}

	// 設定関数を取得する（Version 2.38_beta2以降で対応しているもの）
	g_kmp_cfg = reinterpret_cast<pfnKmpConfig>(GetProcAddress(kpi_inst, SZ_KMP_CONFIG));
	if (g_kmp_cfg) {
		g_has_cfg = true;
	}

	// 名称の設定
	if (kmp_mdl->pszDescription) {
		MultiByteToWideChar(CP_ACP, 0, kmp_mdl->pszDescription, -1, name, MAX_PATH);
	}
	else {
		wsprintf(name, L"KbMedia:%s", PathFindFileName(kpi_path));
	}

	char ext[MAX_PATH];
	ext[0] = '*';
	ext[1] = '\0';

	// 拡張子を取得する
	lstrcatA(ext, kmp_mdl->ppszSupportExts[0]);
	for (int i = 1; kmp_mdl->ppszSupportExts[i]; i++) {
		lstrcatA(ext, ";*");
		lstrcatA(ext, kmp_mdl->ppszSupportExts[i]);
	}

	MultiByteToWideChar(CP_ACP, 0, ext, -1, type, MAX_PATH);

	PathRenameExtension(kpi_path, L".ini");
	if (PathFileExists(kpi_path)) {
		g_has_cfg = true;
	}

	g_kpi_inst = kpi_inst;
	g_kmp_mdl  = kmp_mdl;

	if (g_kmp_mdl && g_kmp_mdl->Init) {
		g_kmp_mdl->Init();
	}

	return true;
}

//-----------------------------------------------------------------------------
// プラグインの解放
//-----------------------------------------------------------------------------
static void FreeKPI()
{
	if (g_kpi_inst) {
		FreeLibrary(g_kpi_inst);
	}

	g_kpi_inst = NULL;
	g_kmp_mdl  = NULL;
	g_kmp_cfg  = NULL;
	g_has_cfg  = false;
}

//-----------------------------------------------------------------------------
// 解放
//-----------------------------------------------------------------------------
static void LPAPI Release()
{
	if (g_kmp_mdl && g_kmp_mdl->Deinit) {
		g_kmp_mdl->Deinit();
	}

	FreeKPI();
}

//-----------------------------------------------------------------------------
// 情報表示
//-----------------------------------------------------------------------------
static void LPAPI Property(HINSTANCE inst, HWND parent)
{
	if (g_kmp_cfg) {
		g_kmp_cfg(parent, 0, 0);
	}
	else {
		wchar_t ini_path[MAX_PATH];

		GetModuleFileName(g_dll_inst, ini_path, MAX_PATH);
		PathRenameExtension(ini_path, L".ini");
		if (PathFileExists(ini_path)) {
			ShellExecute(parent, L"open", ini_path, NULL, NULL, SW_SHOWNORMAL);
		}
	}
}

//-----------------------------------------------------------------------------
// 解析
//-----------------------------------------------------------------------------
static int LPAPI Parse(const wchar_t* path, Metadata* meta)
{
	char buf[MAX_PATH];

	WideCharToMultiByte(CP_ACP, 0, path, -1, buf, MAX_PATH, NULL, NULL);

	SOUNDINFO si;

	RtlZeroMemory(&si, sizeof(si));

	HKMP hkmp = g_kmp_mdl->Open(buf, &si);
	if (!hkmp) {
		return false;
	}

	g_kmp_mdl->Close(hkmp);

	meta->duration = si.dwLength;
	meta->seekable = (si.dwSeekable != 0);
	return true;
}

//-----------------------------------------------------------------------------
// 再生するデータを開く
//-----------------------------------------------------------------------------
static Handle LPAPI Open(const wchar_t* path, Output* out)
{
	if (!g_kmp_mdl) {
		return NULL;
	}

	char buf[MAX_PATH];
	WideCharToMultiByte(CP_ACP, 0, path, -1, buf, MAX_PATH, NULL, NULL);

	SOUNDINFO si;
	RtlZeroMemory(&si, sizeof(si));

	HKMP hkmp = g_kmp_mdl->Open(buf, &si);
	if (!hkmp) {
		return NULL;
	}

	if (si.dwBitsPerSample < 0x80000000) {
		out->sample_bits = si.dwBitsPerSample;
	}
	else {
		g_kmp_mdl->Close(hkmp);
		return NULL;
		//out->sample_bits = (si.dwBitsPerSample & 0x7FFFFFFF);
	}

	out->sample_rate	= si.dwSamplesPerSec;
	out->num_channels	= si.dwChannels;
	out->unit_length	= si.dwUnitRender;

	return hkmp;
}

//-----------------------------------------------------------------------------
// 再生するデータを閉じる
//-----------------------------------------------------------------------------
static void LPAPI Close(Handle handle)
{
	if (handle) {
		g_kmp_mdl->Close(handle);
	}
}

//-----------------------------------------------------------------------------
// レンダリング
//-----------------------------------------------------------------------------
static int LPAPI Render(Handle handle, void* buffer, int size)
{
	if (handle) {
		return g_kmp_mdl->Render(handle, static_cast<BYTE*>(buffer), size);
	}

	return 0;
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
static int LPAPI Seek(Handle handle, int time_ms)
{
	if (handle) {
		return g_kmp_mdl->SetPosition(handle, time_ms);
	}

	return 0;
}

//-----------------------------------------------------------------------------
// プラグインエクスポート関数
//-----------------------------------------------------------------------------
LPEXPORT LunaPlugin* GetLunaPlugin(HINSTANCE /*instance*/)
{
	static LunaPlugin plugin;
	static wchar_t name[MAX_PATH];
	static wchar_t type[MAX_PATH];

	plugin.plugin_kind = KIND_PLUGIN;
	plugin.plugin_name = name;
	plugin.support_type = type;

	if (!LoadKPI(name, type)) {
		return NULL;
	}

	plugin.Release	= Release;
	plugin.Property	= g_has_cfg? Property : NULL;
	plugin.Parse	= Parse;
	plugin.Open		= Open;
	plugin.Close	= Close;
	plugin.Render	= Render;
	plugin.Seek		= Seek;

	return &plugin;
}
