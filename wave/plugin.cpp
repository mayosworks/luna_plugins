//=============================================================================
// WAVEプラグイン実装
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include "luna_pi.h"
#include "wav_reader.h"
#include "aif_reader.h"
#include "snd_reader.h"
#include "caf_reader.h"

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
// new/delete operators
//-----------------------------------------------------------------------------
void* operator new(size_t size)
{
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

void operator delete(void* ptr)
{
	HeapFree(GetProcessHeap(), 0, ptr);
}

// ignore link error
int _purecall()
{
	return 0;
}

//-----------------------------------------------------------------------------
// 解析
//-----------------------------------------------------------------------------
static int LPAPI Parse(const wchar_t* path, Metadata* meta)
{
	if (WavReader::Parse(path, meta)) {
		return true;
	}

	if (AifReader::Parse(path, meta)) {
		return true;
	}

	if (SndReader::Parse(path, meta)) {
		return true;
	}

	if (CafReader::Parse(path, meta)) {
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// 開く
//-----------------------------------------------------------------------------
static Handle LPAPI Open(const wchar_t* path, Output* out)
{
	Reader* reader = new WavReader();
	if (!reader->Open(path)) {
		delete reader;

		reader = new AifReader();
		if (!reader->Open(path)) {
			delete reader;

			reader = new SndReader();
			if (!reader->Open(path)) {
				delete reader;

				reader = new CafReader();
				if (!reader->Open(path)) {
					delete reader;
					return NULL;
				}
			}
		}
	}

	const WAVEFORMATEX& wfx = reader->GetFormat();

	out->sample_rate	= wfx.nSamplesPerSec;
	out->sample_bits	= wfx.wBitsPerSample;
	out->num_channels	= wfx.nChannels;
	out->unit_length	= 0;
	return reader;
}

//-----------------------------------------------------------------------------
// 閉じる
//-----------------------------------------------------------------------------
static void LPAPI Close(Handle handle)
{
	Reader* reader = static_cast<Reader*>(handle);
	if (reader) {
		delete reader;
	}
}

//-----------------------------------------------------------------------------
// レンダリング
//-----------------------------------------------------------------------------
static int LPAPI Render(Handle handle, void* buffer, int size)
{
	Reader* reader = static_cast<Reader*>(handle);
	if (reader) {
		return reader->Read(buffer, size);
	}

	return 0;
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
static int LPAPI Seek(Handle handle, int time_ms)
{
	Reader* reader = static_cast<Reader*>(handle);
	if (reader) {
		return reader->Seek(time_ms);
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
	plugin.plugin_name = L"WAVE plugin v1.04";
	plugin.support_type = L"*.wav;*.aif;*.aiff;*.au;*.snd;*.caf;";

	plugin.Release	= NULL;
	plugin.Property	= NULL;
	plugin.Parse	= Parse;
	plugin.Open		= Open;
	plugin.Close	= Close;
	plugin.Render	= Render;
	plugin.Seek		= Seek;

	return &plugin;
}
