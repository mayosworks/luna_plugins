//=============================================================================
// VST Instruments Host
//=============================================================================

#pragma once

//! @brief	VSTiホスト
class VstiHost
{
public:
	VstiHost();
	~VstiHost();

	//! @brief	VSTi初期化
	bool Init(const wchar_t* vsti_path, bool reset_on_start);

	//! @brief	VSTi破棄
	void Term();

	//! @brief	演奏開始
	bool Start(int sample_rate);

	//! @brief	演奏停止
	void Stop();

	//! @brief	MIDIリセット
	void Reset(const void* reset_data, int data_size);

	//! @brief	MIDIメッセージをレンダリングする
	bool Render(const int* message_data, int message_num, int samples);

	//! @brief	チャンネル０のPCMデータを取得する
	const float* GetChannel0() const;

	//! @brief	チャンネル１のPCMデータを取得する
	const float* GetChannel1() const;

	//! @brief	再生中か？
	bool IsPlaying() const;

	//! @brief	設定エディタを開く
	void ShowEditor(void* inst, void* hwnd);

private:
	class Impl;
	Impl* m_impl;
};
