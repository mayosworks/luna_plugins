//=============================================================================
// MIDIプラグイン実装
//=============================================================================

#define NOMINMAX

#include <windows.h>
#include <mmsystem.h>
#include <ks.h>
#include <mmreg.h>
#include <shlwapi.h>
#include <vector>
#include "luna_pi.h"
#include "smf_loader.h"
#include "vsti_host.h"

//-----------------------------------------------------------------------------
// 定義
//-----------------------------------------------------------------------------
namespace {

// １回のRenderで再生する時間。
const int BLOCK_TIME = 50;

// 何分割してRenderするか。
const int DIVIDE_NUM = 5;

// デフォルト設定。
const int DEFAULT_RATE = 44100;
const int DEFAULT_BITS = 16;

// メッセージリスト
typedef std::vector<int> MsgList;

// 再生時コンテキスト
struct Context
{
	SmfLoader	loader;
	int			midx;
	int			mnum;
	int			time;
	int			tend;
	int			bits;	// レンダリング時のビット数。
};

// グローバルオブジェクト
VstiHost	g_vsti;
bool		g_playing = false;

// クランプ
inline float Clamp(float x, float min, float max)
{
	return (x < min)? min : ((x > max)? max : x);
}

} //namespace

// プロトタイプ宣言
static int RenderMidi(const MsgList& msg, int out_bits, void* buffer, int size);
static int PlayMidi(Context* cxt, void* buffer, int size);

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
// 解放
//-----------------------------------------------------------------------------
static void LPAPI Release()
{
	g_vsti.Term();
}

//-----------------------------------------------------------------------------
// 情報表示
//-----------------------------------------------------------------------------
static void LPAPI Property(HINSTANCE inst, HWND parent)
{
	g_vsti.ShowEditor(inst, parent);
}

//-----------------------------------------------------------------------------
// 解析
//-----------------------------------------------------------------------------
static int LPAPI Parse(const wchar_t* path, Metadata* meta)
{
	SmfLoader loader;

	SmfLoader::LoadOption option;
	option.ignore_bank_select = false;
	option.detect_reset_type = true;

	if (!loader.Load(path, option)) {
		return false;
	}

	meta->duration = loader.GetDuration();
	meta->seekable = true;

	MultiByteToWideChar(CP_ACP, 0, loader.GetTitle(), -1, meta->title, META_MAXLEN);

	static const wchar_t* const RESET_TEXT[] =
	{
		L"GM1", L"GM2", L"XG", L"GS",
	};

	wsprintf(meta->extra, L"SMF%d, %s, TimeBase:%d, Tracks:%d", loader.GetSmfFormat(),
			RESET_TEXT[loader.GetResetType()], loader.GetTimeBase(), loader.GetTrackNum());

	return true;
}

//-----------------------------------------------------------------------------
// 再生するデータを開く
//-----------------------------------------------------------------------------
static Handle Open(const wchar_t* path, Output* out)
{
	// ２重再生は不可
	if (g_playing) {
		return NULL;
	}

	Context* cxt = new Context();
	if (!cxt) {
		return NULL;
	}

	SmfLoader::LoadOption option;
	option.ignore_bank_select = true;
	option.detect_reset_type = true;

	if (!cxt->loader.Load(path, option)) {
		delete cxt;
		return NULL;
	}

	cxt->midx = 0;
	cxt->mnum = cxt->loader.GetMidiMessageNum();
	cxt->time = 0;
	cxt->tend = cxt->loader.GetDuration();
	cxt->bits = DEFAULT_BITS;

	out->sample_rate	= DEFAULT_RATE;
	out->sample_bits	= DEFAULT_BITS;
	out->num_channels	= 2;
	out->unit_length	= MulDiv(out->sample_rate * out->sample_bits / 8 * out->num_channels, BLOCK_TIME, 1000);

	if (!g_vsti.Start(out->sample_rate)) {
		delete cxt;
		return NULL;
	}

	g_vsti.Reset(cxt->loader.GetResetMessageData(), cxt->loader.GetResetMessageSize());

	g_playing = true;
	return cxt;
}

//-----------------------------------------------------------------------------
// 再生するデータを閉じる
//-----------------------------------------------------------------------------
static void Close(Handle handle)
{
	g_vsti.Stop();

	Context* cxt = static_cast<Context*>(handle);
	if (cxt) {
		delete cxt;
	}

	g_playing = false;
}

//-----------------------------------------------------------------------------
// レンダリング
//-----------------------------------------------------------------------------
static int Render(Handle handle, void* buffer, int length)
{
	Context* cxt = static_cast<Context*>(handle);
	if (!cxt) {
		return 0;
	}

	char* outbuf = static_cast<char*>(buffer);
	int bsize = length / DIVIDE_NUM;
	int bused = 0;
	for (int i = 0; i < DIVIDE_NUM; ++i) {
		if (!PlayMidi(cxt, outbuf, bsize)) {
			return bused;
		}

		cxt->time += (BLOCK_TIME / DIVIDE_NUM);

		outbuf += bsize;
		bused += bsize;
	}

	return bused;
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
static int Seek(Handle handle, int time_ms)
{
	Context* cxt = static_cast<Context*>(handle);
	if (!cxt) {
		return 0;
	}

	g_vsti.Reset(cxt->loader.GetResetMessageData(), cxt->loader.GetResetMessageSize());

	if (time_ms < cxt->time) {
		cxt->midx = 0;
	}

	cxt->time = time_ms;

	PlayMidi(cxt, NULL, 0);
	return time_ms;
}

//-----------------------------------------------------------------------------
// プラグインエクスポート関数
//-----------------------------------------------------------------------------
LPEXPORT LunaPlugin* LPAPI GetLunaPlugin(HINSTANCE instance)
{
	TCHAR vsti_path[MAX_PATH];
	GetModuleFileName(instance, vsti_path, MAX_PATH);
	PathRenameExtension(vsti_path, L".dll");

	TCHAR ini_path[MAX_PATH];
	GetModuleFileName(instance, ini_path, MAX_PATH);
	PathRenameExtension(ini_path, L".ini");
	bool reset_on_start = (GetPrivateProfileInt(L"Config", L"ResetOnStart", 0, ini_path) != 0);

	if (!g_vsti.Init(vsti_path, reset_on_start)) {
		return false;
	}

	static TCHAR name[MAX_PATH];
	wsprintf(name, TEXT("VSTi[%s] MIDI plugin v1.00"), PathFindFileName(vsti_path));
	
	static LunaPlugin plugin;

	plugin.plugin_kind = KIND_PLUGIN;
	plugin.plugin_name = name;
	plugin.support_type = L"*.mid";

	plugin.Release	= Release;
	plugin.Property	= Property;
	plugin.Parse	= Parse;
	plugin.Open		= Open;
	plugin.Close	= Close;
	plugin.Render	= Render;
	plugin.Seek		= Seek;

	return &plugin;
}

//-----------------------------------------------------------------------------
// MIDIメッセージを送信し、PCMデータを生成する
//-----------------------------------------------------------------------------
int RenderMidi(const MsgList& msg, int out_bits, void* buffer, int size)
{
	int samples = size / (out_bits / 8) / 2;
	int msg_num = static_cast<int>(msg.size());
	const int* msg_data = (msg_num != 0)? &msg[0] : NULL;

	if (!g_vsti.Render(msg_data, msg_num, samples)) {
		return 0;
	}

	const float* ch0 = g_vsti.GetChannel0();
	const float* ch1 = g_vsti.GetChannel1();

	// 32bit PCM
	if (out_bits == 32) {
		const float S32_MUL =  1503192678.0f;	// 70%
		const float S32_MAX =  2147418112.0f;
		const float S32_MIN = -2147418112.0f;

		int* pcm = static_cast<int*>(buffer);
		for (int i = 0, j = 0; i < samples; ++i) {
			float v0 = Clamp(ch0[i] * S32_MUL, S32_MIN, S32_MAX);
			float v1 = Clamp(ch1[i] * S32_MUL, S32_MIN, S32_MAX);

			pcm[j++] = static_cast<int>(v0);
			pcm[j++] = static_cast<int>(v1);
		}
	}
	// 24bit PCM
	else if (out_bits == 24) {
		const float S24_MUL =  6000000.0f;	// 71%
		const float S24_MAX =  8388607.0f;
		const float S24_MIN = -8388608.0f;

		union Int4Byte
		{
			int		i;
			char	b[4];
		};

		Int4Byte ib;
		char* pcm = static_cast<char*>(buffer);
		for (int i = 0, j = 0; i < samples; ++i) {
			float v0 = Clamp(ch0[i] * S24_MUL, S24_MIN, S24_MAX);
			float v1 = Clamp(ch1[i] * S24_MUL, S24_MIN, S24_MAX);

			ib.i = static_cast<int>(v0);
			pcm[j++] = ib.b[0];
			pcm[j++] = ib.b[1];
			pcm[j++] = ib.b[2];

			ib.i = static_cast<int>(v1);
			pcm[j++] = ib.b[0];
			pcm[j++] = ib.b[1];
			pcm[j++] = ib.b[2];
		}
	}
	// 16bit PCM
	else if (out_bits == 16) {
		const float S16_MUL =  20000.0f;	// 60%
		const float S16_MAX =  32767.0f;
		const float S16_MIN = -32768.0f;

		short* pcm = static_cast<short*>(buffer);
		for (int i = 0, j = 0; i < samples; ++i) {
			float v0 = Clamp(ch0[i] * S16_MUL, S16_MIN, S16_MAX);
			float v1 = Clamp(ch1[i] * S16_MUL, S16_MIN, S16_MAX);

			pcm[j++] = static_cast<short>(v0);
			pcm[j++] = static_cast<short>(v1);
		}
	}
	// unsupported
	else {
		size = 0;
	}

	return size;
}

//-----------------------------------------------------------------------------
// MIDI再生
//-----------------------------------------------------------------------------
int PlayMidi(Context* cxt, void* buffer, int size)
{
	MsgList msglist;

	// 最後までいっている場合は、残りのサンプルを取得するだけ。
	if (cxt->midx == cxt->mnum) {
		if (cxt->time < cxt->tend) {
			return RenderMidi(msglist, cxt->bits, buffer, size);
		}

		return 0;
	}

	msglist.reserve(128);

	// 現在の演奏時間までのメッセージを全て送信する
	int time = cxt->loader.GetMidiMessage(cxt->midx).time;
	while ((cxt->midx < cxt->mnum) && (time <= cxt->time)) {
		int msg = cxt->loader.GetMidiMessage(cxt->midx).data;
		if (msg != SmfLoader::END_OF_TRACK) {
			int type = (msg & 0xF0);
			int mval = ((msg >> 8) & 0xFF);

			// バンクセレクトを無視(0x00:bank select MSB/0x20:bank select LSB)
			if (type != 0xB0 || (mval != 0x00 && mval != 0x20)) {
				msglist.push_back(msg);
			}
		}

		if (++cxt->midx < cxt->mnum) {
			time = cxt->loader.GetMidiMessage(cxt->midx).time;
		}
	}

	return RenderMidi(msglist, cxt->bits, buffer, size);
}
