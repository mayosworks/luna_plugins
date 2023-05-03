﻿//=============================================================================
// Lunaプラグイン インターフェイス (2016/09/03版)
//                                                Copyright (c) 2006-2016 MAYO.
//=============================================================================
#pragma once
#pragma pack(push, 4)

//-----------------------------------------------------------------------------
// 型定義
//-----------------------------------------------------------------------------
#if defined(_WIN32) && !defined(_WIN64)
#define LPAPI __cdecl	// 呼び出し規約は__cdecl(x86-32bitのみ)
#else //defined(_WIN32) && !defined(_WIN64) 
#define LPAPI			// 呼び出し規約なし
#endif //defined(_WIN32) && !defined(_WIN64)

typedef void* Handle;	// 再生時に使用するハンドル

//-----------------------------------------------------------------------------
// 定数定義
//-----------------------------------------------------------------------------
#define	KIND_PLUGIN		0x00010000	// プラグイン種類
#define	META_MAXLEN		255			// 文字情報の最大文字数

//-----------------------------------------------------------------------------
// メタデータ
//-----------------------------------------------------------------------------
typedef struct
{
	int		duration;					// 長さ、ミリ秒単位
	int		seekable;					// シークが可能か？1:可、0:不可
	wchar_t	title [META_MAXLEN + 1];	// タイトル
	wchar_t	artist[META_MAXLEN + 1];	// アーティスト
	wchar_t	album [META_MAXLEN + 1];	// アルバム、リリース年
	wchar_t	extra [META_MAXLEN + 1];	// 追加情報、ジャンルや形式情報等
}
Metadata;

//-----------------------------------------------------------------------------
// オーディオ出力設定
// ・PCM/INT, 8/16/24/32bitに対応しています。
// ・24bit出力の場合は、パディングを入れないようにしてください。
//-----------------------------------------------------------------------------
typedef struct
{
	int		sample_rate;	// サンプルレート(Hz)
	int		sample_bits;	// サンプルのビット数(8/16/24/32bit)
	int		num_channels;	// チャンネル数(1,2,...)
	int		unit_length;	// Render()に渡す単位バイト数、0なら任意となる。
}
Output;

//-----------------------------------------------------------------------------
// プラグイン構造体
// ・Config以外の関数すべて実装が必須です。
// ・読み取りに関係する部分は、別スレッドから呼び出します。
// ・設定値を読み取り中に反映する場合は、プラグイン側で排他処理をしてください。
//-----------------------------------------------------------------------------
typedef struct
{
	//-------------------------------------------------------------------------
	// 基本情報
	//-------------------------------------------------------------------------
	int				plugin_kind;	// KIND_PLUGINを指定
	const wchar_t*	plugin_name;	// プラグイン名称（例："WAVE plugin ver 1.00"）
	const wchar_t*	support_type;	// 対応形式（例："*.wav;*.pcm"）

	//-------------------------------------------------------------------------
	// プラグイン解放
	//
	// Params:	なし
	//
	// Returns:	なし
	//-------------------------------------------------------------------------
	void (LPAPI* Release)(void);

	//-------------------------------------------------------------------------
	// 設定や権利情報等のダイアログ表示
	// ・parentを親ウィンドウとして設定してください、
	// ・不要な場合は、NULLにしてください。
	//
	// Params:	instance	DLLのインスタンスハンドル
	//			parent		親にするウィンドウハンドル
	//
	// Returns:	なし
	//-------------------------------------------------------------------------
	void (LPAPI* Property)(HINSTANCE instance, HWND parent);

	//-------------------------------------------------------------------------
	// 対象のデータを解析し、メタデータを取得
	//
	// Params:	path	対象パス
	//			mode	解析モード
	//			meta	メタ情報格納先
	//
	// Returns:	解析・再生できる時 1
	//			解析・再生できない時 0
	//-------------------------------------------------------------------------
	int (LPAPI* Parse)(const wchar_t* path, Metadata* meta);

	//-------------------------------------------------------------------------
	// 再生対象データを開く
	//
	// Params:	path	対象パス
	//			out		出力設定
	//
	// @return	再生ハンドル（任意のポインタ）、成功時はNULL以外を設定のこと
	//			エラー時はNULLを設定のこと
	//-------------------------------------------------------------------------
	Handle (LPAPI* Open)(const wchar_t* path, Output* out);

	//-------------------------------------------------------------------------
	// 再生対象データを閉じる
	//
	// Params:	handle	再生ハンドル
	//
	// Returns:	なし
	//-------------------------------------------------------------------------
	void (LPAPI* Close)(Handle handle);

	//-------------------------------------------------------------------------
	// PCMデータ生成／読み取り
	// ・戻り値がsizeより小さくなったときに、終了と判断します
	//
	// Params:	handle	再生ハンドル
	//			buffer	出力先バッファ
	//			length	読み取るバイト数
	//
	// Returns:	バッファに書き込んだバイト数
	//-------------------------------------------------------------------------
	int (LPAPI* Render)(Handle handle, void* buffer, int length);

	//-------------------------------------------------------------------------
	// 生成／読み取り位置設定
	// ・シークできない場合、0で呼び出しますので、最初の位置にしてください
	//
	// Params:	handle	再生ハンドル
	//			time_ms	ミリ秒単位のシーク時間
	//
	// Returns:	設定できたミリ秒単位の時間
	//-------------------------------------------------------------------------
	int (LPAPI* Seek)(Handle handle, int time_ms);
}
LunaPlugin;


//-----------------------------------------------------------------------------
// エクスポート関数：プラグイン取得
// ・"GetLunaPlugin"という名前でエクスポートしてください。
// ・返すポインタは、Release()が呼び出されるまで保持してください。
// ・初期化等が必要であれば、ここで行ってください。
//
// Params:	instance	DLLのインスタンスハンドル
//
// Returns:	プラグイン構造体へのポインタ、エラー等の場合はNULL
//-----------------------------------------------------------------------------
#if defined(__cplusplus)
#define LPEXPORT extern "C" __declspec(dllexport)
#else //defined(__cplusplus)
#define LPEXPORT extern __declspec(dllexport)
#endif //defined(__cplusplus)

LPEXPORT LunaPlugin* LPAPI GetLunaPlugin(HINSTANCE instance);

#pragma pack(pop)

