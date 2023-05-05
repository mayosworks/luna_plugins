//=============================================================================
// VST Instruments Host
//=============================================================================

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "vsti_host.h"

#include <windows.h>
#include "./vstsdk/aeffectx.h"
#include "resource.h"

// 内部定義
namespace {

// Vender/Product文字列
const char VENDOR[] = "YAMAHA";
const char PRODUCT[] = "Cubase VST";

// バッファするサンプル数
// １回に10ms分レンダリングするとして96kHzなら960サンプル、なのでそれ以上の数値
const int BUF_SAMPLES = 1024;

} //namespace

// VST Instruments 内部クラス 
class VstiHost::Impl
{
public:
	Impl();
	~Impl();

	bool Init(const wchar_t* vsti_path, bool reset_on_start);
	void Term();

	bool Start(int sample_rate);
	void Stop();

	void Reset(const void* reset_data, int data_size);
	bool Render(const int* message_data, int message_num, int samples);

	const float* GetChannel0() const { return m_output[0]; }
	const float* GetChannel1() const { return m_output[1]; }

	bool IsPlaying() const { return m_playing; }

	void ShowEditor(HINSTANCE inst, HWND hwnd);

private:
	static VstIntPtr VSTCALLBACK HostCallback(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
	VstIntPtr Dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);

	static INT_PTR CALLBACK DlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);

private:
	static const int OUTPUT_NUM = 2;

	typedef AEffect* (*EntryProc)(audioMasterCallback);

	HMODULE		m_module;
	EntryProc	m_e_proc;
	AEffect*	m_effect;
	float*		m_output[OUTPUT_NUM];
	float*		m_buffer;
	bool		m_reset_on_start;
	bool		m_playing;
};


//-----------------------------------------------------------------------------
// コンストラクタ
//-----------------------------------------------------------------------------
VstiHost::Impl::Impl()
	: m_module(NULL)
	, m_e_proc(NULL)
	, m_effect(NULL)
	, m_buffer(NULL)
	, m_reset_on_start(false)
	, m_playing(false)
{
	m_output[0] = NULL;
	m_output[1] = NULL;
}

//-----------------------------------------------------------------------------
// デストラクタ
//-----------------------------------------------------------------------------
VstiHost::Impl::~Impl()
{
}

//-----------------------------------------------------------------------------
// シンセ初期化
//-----------------------------------------------------------------------------
bool VstiHost::Impl::Init(const wchar_t* vsti_path, bool reset_on_start)
{
	m_reset_on_start = reset_on_start;

	// クリティカルエラーなどのメッセージを非表示化
	UINT err_mode = SetErrorMode(SEM_FAILCRITICALERRORS);

	m_module = LoadLibraryExW(vsti_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

	// エラーモードを戻す
	SetErrorMode(err_mode);

	if (!m_module) {
		return false;
	}

	// "VSTPluginMain"の場合もあるが、とりあえず・・・
	m_e_proc = reinterpret_cast<EntryProc>(GetProcAddress(m_module, "main"));
	if (!m_e_proc) {
		Term();
		return false;
	}

	m_buffer = new float[OUTPUT_NUM * BUF_SAMPLES];
	if (!m_buffer) {
		Term();
		return false;
	}

	for (int i = 0; i < OUTPUT_NUM; ++i) 	{
		m_output[i] = &m_buffer[i * BUF_SAMPLES];
	}

	if (!reset_on_start) {
		m_effect = m_e_proc(HostCallback);
		if (!m_effect) {
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// シンセ終了
//-----------------------------------------------------------------------------
void VstiHost::Impl::Term()
{
	if (m_buffer) {
		delete [] m_buffer;
		m_buffer = NULL;
	}

	if (m_module) {
		FreeLibrary(m_module);
		m_module = NULL;
	}

	m_effect = NULL;
	m_e_proc = NULL;

	m_output[0] = NULL;
	m_output[1] = NULL;
}

//-----------------------------------------------------------------------------
// 演奏開始
//-----------------------------------------------------------------------------
bool VstiHost::Impl::Start(int sample_rate)
{
	if (m_reset_on_start) {
		m_effect = m_e_proc(HostCallback);
		if (!m_effect) {
			return false;
		}

		Dispatcher(effOpen, 0, 0, NULL, 0.0f);
	}

	float rate_param = static_cast<float>(sample_rate);

	Dispatcher(effSetProgram, 0, 0, NULL, 0.0f);
	Dispatcher(effSetSampleRate, 0, 0, NULL, rate_param);
	Dispatcher(effSetBlockSize, 0, BUF_SAMPLES, NULL, 0.0f);
	Dispatcher(effMainsChanged, 0, 1, NULL, 0.0f);
	Dispatcher(effSetProgram, 0, 0, NULL, 0.0f);

	m_playing = true;
	return true;
}

//-----------------------------------------------------------------------------
// 演奏停止
//-----------------------------------------------------------------------------
void VstiHost::Impl::Stop()
{
	Dispatcher(effStopProcess, 0, 0, NULL, 0.0f);
	Dispatcher(effMainsChanged, 0, 0, NULL, 0.0f);

	if (m_reset_on_start) {
		Dispatcher(effClose, 0, 0, NULL, 0.0f);
		m_effect = NULL;
	}

	m_playing = false;
}

//-----------------------------------------------------------------------------
// MIDIリセット
//-----------------------------------------------------------------------------
void VstiHost::Impl::Reset(const void* reset_data, int data_size)
{
	VstEvents event;
	VstMidiSysexEvent sysex;

	memset(&event, 0, sizeof(event));
	memset(&sysex, 0, sizeof(sysex));

	event.numEvents = 1;
	event.reserved = 0xBAADF00D;
	event.events[0] = reinterpret_cast<VstEvent*>(&sysex);

	sysex.type = kVstSysExType;
	sysex.byteSize = sizeof(sysex);
	sysex.dumpBytes = data_size;
	sysex.sysexDump = const_cast<char*>(static_cast<const char*>(reset_data));

	Dispatcher(effStopProcess, 0, 0, NULL, 0.0f);
	Dispatcher(effMainsChanged, 0, 0, NULL, 0.0f);
	Dispatcher(effProcessEvents, 0, 0, &event, 0.0f);
	Sleep(50);

	Dispatcher(effMainsChanged, 0, 1, NULL, 0.0f);
	Dispatcher(effSetProgram, 0, 0, NULL, 0.0f);

	memset(m_buffer, 0, OUTPUT_NUM * BUF_SAMPLES * sizeof(float));
}

//-----------------------------------------------------------------------------
// シンセ実行（buffer==NULLなら出力なしで処理）
//-----------------------------------------------------------------------------
bool VstiHost::Impl::Render(const int* message_data, int message_num, int samples)
{
	const size_t SEND_EVENT_SIZE = sizeof(VstEvents) + sizeof(VstEvent*) * message_num;
	const size_t MIDI_EVENT_SIZE = sizeof(VstEvent) * message_num + 4;

	HANDLE heap = GetProcessHeap();
	VstEvents* send_event = static_cast<VstEvents*>(HeapAlloc(heap, HEAP_ZERO_MEMORY, SEND_EVENT_SIZE));
	if (!send_event) {
		return false;
	}

	VstMidiEvent* midi_event = static_cast<VstMidiEvent*>(HeapAlloc(heap, HEAP_ZERO_MEMORY, MIDI_EVENT_SIZE));
	if (!midi_event) {
		HeapFree(heap, 0, send_event);
		return false;
	}

	send_event->numEvents = message_num;
	send_event->reserved = 0xBAADF00D;

	for (int i = 0; i < message_num; ++i) {
		int mmsg = message_data[i];
		int type = (mmsg & 0xF0);

		VstMidiEvent* e = &midi_event[i];
		send_event->events[i] = reinterpret_cast<VstEvent*>(e);

		e->type = kVstMidiType;
		e->byteSize = sizeof(VstMidiEvent);
		e->noteLength = ((type == 0xC0) || (type == 0xD0))? 2 : 3;

		memcpy(e->midiData, &mmsg, sizeof(mmsg));
	}

	Dispatcher(effProcessEvents, 0, 0, send_event, 0.0f);

	if (0 < samples) {
		memset(m_buffer, 0, OUTPUT_NUM * BUF_SAMPLES * sizeof(float));
		m_effect->processReplacing(m_effect, NULL, m_output, samples);
	}

	HeapFree(heap, 0, midi_event);
	HeapFree(heap, 0, send_event);
	return true;
}

//-----------------------------------------------------------------------------
// VSTi設定エディタ
//-----------------------------------------------------------------------------
void VstiHost::Impl::ShowEditor(HINSTANCE inst, HWND hwnd)
{
	if (!hwnd) {
		hwnd = GetForegroundWindow();
	}

	// エフェクトを再取得
	if (!m_effect) {
		m_effect = m_e_proc(HostCallback);
	}

	if (m_effect && (m_effect->flags & effFlagsHasEditor)) {
		LPARAM lp = reinterpret_cast<LPARAM>(this);
		DialogBoxParam(inst, MAKEINTRESOURCE(IDD_HOST), hwnd, DlgProc, lp);
	}
	else {
		MessageBox(hwnd, TEXT("This plugin does not have a editor!"), TEXT("VSTi Host"), MB_OK);
	}
}

//-----------------------------------------------------------------------------
// VSTi側からのコールバック
//-----------------------------------------------------------------------------
VstIntPtr VstiHost::Impl::HostCallback(AEffect* /*effect*/, VstInt32 opcode, VstInt32 /*index*/, VstIntPtr /*value*/, void* ptr, float /*opt*/)
{
	VstIntPtr result = 0;

	switch (opcode) {
	case audioMasterVersion:
		result = kVstVersion;
		break;

	case audioMasterIdle:
		result = 1;
		break;

	case audioMasterGetVendorString:
		// MidRadioのSGP(2).DLLを動かすためには、YAMAHAでないといけない。
		memcpy(ptr, VENDOR, sizeof(VENDOR));
		break;

	case audioMasterGetProductString:
		memcpy(ptr, PRODUCT, sizeof(PRODUCT));
		break;
	}

	return result;
}

//-----------------------------------------------------------------------------
// dispatcherの呼び出しラッパー
//-----------------------------------------------------------------------------
VstIntPtr VstiHost::Impl::Dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
{
	return m_effect->dispatcher(m_effect, opcode, index, value, ptr, opt);
}

//-----------------------------------------------------------------------------
// ダイアログコールバック
//-----------------------------------------------------------------------------
INT_PTR VstiHost::Impl::DlgProc( HWND dlg, UINT msg, WPARAM wp, LPARAM lp )
{
	static VstiHost::Impl* host = NULL;

	if (msg == WM_INITDIALOG) {
		host = reinterpret_cast<VstiHost::Impl*>(lp);

		ERect* rt = NULL;
		host->Dispatcher(effEditGetRect, 0, 0, &rt, 0.0f);
		if (rt) {
			RECT wrt = {0};
			GetWindowRect(dlg, &wrt);

			RECT crt = {0};
			GetClientRect(dlg, &crt);

			int sx = rt->right - rt->left + ((wrt.right - wrt.left) - (crt.right - crt.left));
			int sy = rt->bottom - rt->top + ((wrt.bottom - wrt.top) - (crt.bottom - crt.top));
			SetWindowPos(dlg, HWND_TOP, 0, 0, sx, sy, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE);
		}

		host->Dispatcher(effEditOpen, 0, 0, dlg, 0.0f);

		//wchar_t name[MAX_PATH];
		//host->GetName(name);
		//SetWindowText(dlg, name);
		return TRUE;
	}

	if (msg == WM_COMMAND && LOWORD(wp) == IDCANCEL) {
		host->Dispatcher(effEditClose, 0, 0, dlg, 0.0f);
		host = NULL;

		EndDialog(dlg, 0);
		return TRUE;
	}

	return FALSE;
}

VstiHost::VstiHost()
	: m_impl(new Impl())
{
}

VstiHost::~VstiHost()
{
	delete m_impl;
	m_impl = NULL;
}

bool VstiHost::Init(const wchar_t* vsti_path, bool reset_on_start)
{
	return m_impl->Init(vsti_path, reset_on_start);
}

void VstiHost::Term()
{
	m_impl->Term();
}

bool VstiHost::Start(int sample_rate)
{
	return m_impl->Start(sample_rate);
}

void VstiHost::Stop()
{
	m_impl->Stop();
}

void VstiHost::Reset(const void* reset_data, int data_size)
{
	m_impl->Reset(reset_data, data_size);
}

bool VstiHost::Render(const int* message_data, int message_num, int samples)
{
	return m_impl->Render(message_data, message_num, samples);
}

const float* VstiHost::GetChannel0() const
{
	return m_impl->GetChannel0();
}

const float* VstiHost::GetChannel1() const
{
	return m_impl->GetChannel1();
}

bool VstiHost::IsPlaying() const
{
	return m_impl->IsPlaying();
}

void VstiHost::ShowEditor(void* inst, void* hwnd)
{
	m_impl->ShowEditor(static_cast<HINSTANCE>(inst), static_cast<HWND>(hwnd));
}
