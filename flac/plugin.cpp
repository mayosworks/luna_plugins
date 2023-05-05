//=============================================================================
// Flacプラグイン実装
//=============================================================================

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include "FLAC/stream_decoder.h"
#include "FLAC/metadata.h"
#include "luna_pi.h"

//-----------------------------------------------------------------------------
// 定義
//-----------------------------------------------------------------------------
namespace {

// コンテキスト前方宣言
struct Context;

// レンダリング関数
typedef UINT (*RenderProc)(UINT blocksize, UINT channels, const FLAC__int32* const data[], void* dest);

// FLACのclient_dataに渡すオブジェクト
struct MediaData
{
	HANDLE		file;
	Metadata*	meta;
	bool		ret;
};

// 再生時コンテキスト
struct Context
{
	HANDLE					file;
	FLAC__StreamDecoder*	decoder;
	Output*					out;
	int						samples;
	void*					buffer;
	int						size;
	int						used;
	RenderProc				proc;
};

union Int4Byte
{
	int		i;
	BYTE	b[4];
};

} //namespace

// プロトタイプ宣言

// FLAC関連コールバック
static FLAC__StreamDecoderReadStatus ReadInfo(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
static FLAC__StreamDecoderWriteStatus WriteInfo(const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data);
static void MetaInfo(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static FLAC__StreamDecoderReadStatus ReadData(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
static FLAC__StreamDecoderSeekStatus FileSeek(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
static FLAC__StreamDecoderTellStatus FileTell(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
static FLAC__StreamDecoderLengthStatus FileLength(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
static FLAC__bool FileIsEof(const FLAC__StreamDecoder *decoder, void *client_data);
static FLAC__StreamDecoderWriteStatus WriteData(const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data);
static void MetaData(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void OnError(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

// タグ内容をUNICODEで取得
static bool get_tag(wchar_t* buf, int bufLen, const FLAC__StreamMetadata* meta, const char* key);

// 各ビット数に対応したレンダリング関数
static UINT Render8 (UINT blocksize, UINT channels, const FLAC__int32* const data[], void* dest);
static UINT Render16(UINT blocksize, UINT channels, const FLAC__int32* const data[], void* dest);
static UINT Render20(UINT blocksize, UINT channels, const FLAC__int32* const data[], void* dest);
static UINT Render24(UINT blocksize, UINT channels, const FLAC__int32* const data[], void* dest);
static UINT Render32(UINT blocksize, UINT channels, const FLAC__int32* const data[], void* dest);

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
	const wchar_t* const COPYRIGHT = 
		L"このプラグインは、libFLAC Version 1.3.1 を使用しています。\n\n" \
		L"FLAC - Free Lossless Audio Codec\n" \
		L"Copyright (C) 2001-2009  Josh Coalson\n" \
		L"Copyright (C) 2011-2014  Xiph.Org Foundation";

	MessageBox(parent, COPYRIGHT, L"Property", MB_ICONINFORMATION | MB_OK);
}

//-----------------------------------------------------------------------------
// 解析
//-----------------------------------------------------------------------------
static int LPAPI Parse(const wchar_t* path, Metadata* meta)
{
	HANDLE file = CreateFile(path, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	FLAC__StreamDecoder* decoder = FLAC__stream_decoder_new();
	if (!decoder) {
		CloseHandle(file);
		return false;
	}

	FLAC__stream_decoder_set_md5_checking(decoder, false);
	FLAC__stream_decoder_set_metadata_ignore_all(decoder);
	FLAC__stream_decoder_set_metadata_respond(decoder, FLAC__METADATA_TYPE_STREAMINFO);
	FLAC__stream_decoder_set_metadata_respond(decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);

	MediaData md;
	md.file = file;
	md.meta = meta;
	md.ret = false;

	FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_stream(decoder,
		ReadInfo, NULL, NULL, NULL, NULL, WriteInfo, MetaInfo, OnError, &md);
	if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		FLAC__stream_decoder_delete(decoder);
		CloseHandle(file);
		return false;
	}

	// OGGコンテナに入っているFLACならこっち、ただし、このプラグインでは対応しない
	//statis = FLAC__stream_decoder_init_ogg_stream( decoder, ReadCallback, SeekCallback, TellCallback,
	//	LengthCallback, EofCallback, WriteCallback, MetadataCallback, ErrorCallback, file );

	if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder)) {
		FLAC__stream_decoder_finish(decoder);
		FLAC__stream_decoder_delete(decoder);
		CloseHandle(file);
		return false;
	}

	FLAC__stream_decoder_finish(decoder);
	FLAC__stream_decoder_delete(decoder);
	CloseHandle(file);

	return md.ret;
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

	FLAC__StreamDecoder* decoder = FLAC__stream_decoder_new();
	if (!decoder) {
		CloseHandle(file);
		return NULL;
	}

	FLAC__stream_decoder_set_md5_checking(decoder, false);
	FLAC__stream_decoder_set_metadata_ignore_all(decoder);
	FLAC__stream_decoder_set_metadata_respond(decoder, FLAC__METADATA_TYPE_STREAMINFO);

	Context* cxt = new Context();
	if (!cxt) {
		FLAC__stream_decoder_delete(decoder);
		CloseHandle(file);
		return NULL;
	}

	cxt->file = file;
	cxt->decoder = decoder;
	cxt->out = out;
	cxt->samples = 0;
	cxt->buffer = NULL;
	cxt->size = 0;
	cxt->used = 0;
	cxt->proc = NULL;

	FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_stream(decoder,
		ReadData, FileSeek, FileTell, FileLength, FileIsEof, WriteData, MetaData, OnError, cxt);
	if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		delete cxt;
		FLAC__stream_decoder_delete(decoder);
		CloseHandle(file);
		return NULL;
	}

	if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder)) {
		delete cxt;
		FLAC__stream_decoder_finish(decoder);
		FLAC__stream_decoder_delete(decoder);
		CloseHandle(file);
		return NULL;
	}

	switch (out->sample_bits) {
	case 8 : cxt->proc = Render8 ; break;
	case 16: cxt->proc = Render16; break;
	case 20: cxt->proc = Render20; break;
	case 24: cxt->proc = Render24; break;
	case 32: cxt->proc = Render32; break;

	default:
		// 上記ビット数以外は対応しない。
		delete cxt;
		FLAC__stream_decoder_finish(decoder);
		FLAC__stream_decoder_delete(decoder);
		CloseHandle(file);
		return NULL;
	}

	// 20bitは24bitとして処理
	if (out->sample_bits == 20) {
		out->sample_bits = 24;
	}

	// 最終的なブロックサイズを設定
	out->unit_length *= (out->num_channels * out->sample_bits / 8);

	cxt->out = NULL;
	cxt->samples = out->sample_rate;
	return cxt;
}

//-----------------------------------------------------------------------------
// 再生するデータを閉じる
//-----------------------------------------------------------------------------
static void LPAPI Close(Handle handle)
{
	Context* cxt = static_cast<Context*>(handle);
	if (cxt) {
		if (cxt->decoder) {
			FLAC__stream_decoder_finish(cxt->decoder);
			FLAC__stream_decoder_delete(cxt->decoder);
		}

		if (cxt->file != INVALID_HANDLE_VALUE) {
			CloseHandle(cxt->file);
		}

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

	if (FLAC__stream_decoder_get_state(cxt->decoder) == FLAC__STREAM_DECODER_END_OF_STREAM) {
		return 0;
	}

	cxt->buffer = buffer;
	cxt->size = size;
	cxt->used = 0;

	if (!FLAC__stream_decoder_process_single(cxt->decoder)) {
		return cxt->used;
	}

	return cxt->used;
}

//-----------------------------------------------------------------------------
// シーク
//-----------------------------------------------------------------------------
static int LPAPI Seek(Handle handle, int time_ms)
{
	Context* cxt = static_cast<Context*>(handle);
	if (cxt) {
		int sample = MulDiv(time_ms, cxt->samples, 1000);
		if (FLAC__stream_decoder_seek_absolute(cxt->decoder, sample)) {
			return time_ms;
		}
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
	plugin.plugin_name = L"FLAC plugin v1.03";
	plugin.support_type = L"*.flac";

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
// ファイル読み取り
//-----------------------------------------------------------------------------
FLAC__StreamDecoderReadStatus ReadInfo(
	const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t* bytes, void* client_data)
{
	MediaData* md = static_cast<MediaData*>(client_data);

	DWORD readed = 0;
	if (!ReadFile(md->file, buffer, *bytes, &readed, NULL)) {
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}

	*bytes = readed;
	if (readed == 0) {
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	}

	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

//-----------------------------------------------------------------------------
// 情報書き込み
//-----------------------------------------------------------------------------
FLAC__StreamDecoderWriteStatus WriteInfo(const FLAC__StreamDecoder* decoder,
	const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data)
{
	//MediaData* md = static_cast<ClientData*>(client_data);
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

//-----------------------------------------------------------------------------
// メタデータ発見時
//-----------------------------------------------------------------------------
void MetaInfo(const FLAC__StreamDecoder* decoder, const FLAC__StreamMetadata* metadata, void* client_data)
{
	MediaData* md = static_cast<MediaData*>(client_data);
	Metadata* meta = md->meta;

	// ストリーム情報ブロック（必須）
	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
		const FLAC__StreamMetadata_StreamInfo& info = metadata->data.stream_info;

		// 2chまでサポート
		if (info.channels > 2) {
			md->ret = false;
			return;
		}

		meta->duration = MulDiv(int(info.total_samples), 1000, info.sample_rate);
		meta->seekable = true;

		wsprintf(meta->extra, L"FLAC %dkbps",
			MulDiv(GetFileSize(md->file, NULL), 8, meta->duration));
		md->ret = true;
	}

	// コメントブロック（オプション）
	if (metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
		get_tag(meta->title,  META_MAXLEN, metadata, "TITLE");
		get_tag(meta->artist, META_MAXLEN, metadata, "ARTIST");
		get_tag(meta->album,  META_MAXLEN, metadata, "ALBUM");
	}
}

//-----------------------------------------------------------------------------
// ファイル読み取り
//-----------------------------------------------------------------------------
FLAC__StreamDecoderReadStatus ReadData(
	const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes, void* client_data)
{
	Context* cxt = static_cast<Context*>(client_data);

	DWORD readed = 0;
	if (!ReadFile(cxt->file, buffer, *bytes, &readed, NULL)) {
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}

	*bytes = readed;
	if (readed == 0) {
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	}

	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

//-----------------------------------------------------------------------------
// ファイルポインタ設定
//-----------------------------------------------------------------------------
FLAC__StreamDecoderSeekStatus FileSeek(
	const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	Context* cxt = static_cast<Context*>(client_data);

	SetFilePointer(cxt->file, static_cast<LONG>(absolute_byte_offset), NULL, FILE_BEGIN);
	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

//-----------------------------------------------------------------------------
// ファイルポインタ取得
//-----------------------------------------------------------------------------
FLAC__StreamDecoderTellStatus FileTell(
	const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	Context* cxt = static_cast<Context*>(client_data);

	*absolute_byte_offset = SetFilePointer(cxt->file, 0, NULL, FILE_CURRENT);
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

//-----------------------------------------------------------------------------
// ファイルサイズ取得
//-----------------------------------------------------------------------------
FLAC__StreamDecoderLengthStatus FileLength(
	const FLAC__StreamDecoder* decoder, FLAC__uint64* stream_length, void* client_data)
{
	Context* cxt = static_cast<Context*>(client_data);

	*stream_length = GetFileSize(cxt->file, NULL);
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

//-----------------------------------------------------------------------------
// ファイルの終端？
//-----------------------------------------------------------------------------
FLAC__bool FileIsEof(const FLAC__StreamDecoder* decoder, void* client_data)
{
	Context* cxt = static_cast<Context*>(client_data);

	// ファイルサイズ以上の位置にファイルポインタがあればEOFがtrue
	return SetFilePointer(cxt->file, 0, NULL, FILE_CURRENT) >= GetFileSize(cxt->file, NULL);
}

//-----------------------------------------------------------------------------
// フレームデータ出力
//-----------------------------------------------------------------------------
FLAC__StreamDecoderWriteStatus WriteData(const FLAC__StreamDecoder* decoder,
	const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data)
{
	Context* cxt = static_cast<Context*>(client_data);
	if (cxt->buffer) {
		BYTE* dest = static_cast<BYTE*>(cxt->buffer) + cxt->used;
		cxt->used += cxt->proc(frame->header.blocksize, frame->header.channels, buffer, dest);
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

//-----------------------------------------------------------------------------
// メタデータ発見時
//-----------------------------------------------------------------------------
void MetaData(const FLAC__StreamDecoder* decoder, const FLAC__StreamMetadata* metadata, void* client_data)
{
	Context* cxt = static_cast<Context*>(client_data);
	Output* out = cxt->out;

	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
		const FLAC__StreamMetadata_StreamInfo& info = metadata->data.stream_info;
		// ブロックサイズが同じ場合だけデコードを許可
		if (info.max_blocksize == info.min_blocksize) {
			out->sample_rate	= info.sample_rate;
			out->sample_bits	= info.bits_per_sample;
			out->num_channels	= info.channels;
			out->unit_length	= info.max_blocksize;
		}
	}
}

//-----------------------------------------------------------------------------
// エラー発生時
//-----------------------------------------------------------------------------
void OnError(const FLAC__StreamDecoder* decoder, FLAC__StreamDecoderErrorStatus status, void* client_data)
{
#ifdef _DEBUG
	wchar_t buf[64];
	wsprintf(buf, L"エラーが発生しました。Status:%d", status);
	MessageBox(GetForegroundWindow(), buf, NULL, MB_ICONERROR);
#endif //_DEBUG
}

//-----------------------------------------------------------------------------
// ストリームメタデータからタグを取得する
//-----------------------------------------------------------------------------
bool get_tag(wchar_t* dest_buf, int buf_len, const FLAC__StreamMetadata* meta, const char* key)
{
	int i = FLAC__metadata_object_vorbiscomment_find_entry_from(meta, 0, key);
	if (i >= 0) {
		void* entry = meta->data.vorbis_comment.comments[i].entry;
		const char* tag = strchr(static_cast<char*>(entry), '=');
		if (tag) {
			MultiByteToWideChar(CP_UTF8, 0, tag + 1, -1, dest_buf, buf_len);
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// 8bitレンダリング
//-----------------------------------------------------------------------------
UINT Render8(UINT blocksize, UINT channels, const FLAC__int32* const data[], void* dest)
{
	UINT i = 0;

	signed char* outbuf = static_cast<signed char*>(dest);
	for (UINT s = 0; s < blocksize; ++s ) {
		for (UINT ch = 0; ch < channels; ++ch) {
			outbuf[i++] = static_cast<signed char>(data[ch][s]);
		}
	}

	return i;
}

//-----------------------------------------------------------------------------
// 16bitレンダリング
//-----------------------------------------------------------------------------
UINT Render16(UINT blocksize, UINT channels, const FLAC__int32* const data[], void* dest)
{
	UINT i = 0;

	short* outbuf = static_cast<short*>(dest);
	for (UINT s = 0; s < blocksize; ++s) {
		for (UINT ch = 0; ch < channels; ++ch) {
			outbuf[i++] = static_cast<short>(data[ch][s]);
		}
	}

	return i * sizeof(short);
}

//-----------------------------------------------------------------------------
// 20bitレンダリング
//-----------------------------------------------------------------------------
UINT Render20(UINT blocksize, UINT channels, const FLAC__int32* const data[], void* dest)
{
	Int4Byte ib;
	UINT i = 0;

	BYTE* outbuf = static_cast<BYTE*>(dest);
	for (UINT s = 0; s < blocksize; ++s) {
		for (UINT ch = 0; ch < channels; ++ch) {
			ib.i = data[ch][s] * 16;
			outbuf[i++] = ib.b[0];
			outbuf[i++] = ib.b[1];
			outbuf[i++] = ib.b[2];
		}
	}

	return i;
}

//-----------------------------------------------------------------------------
// 24bitレンダリング
//-----------------------------------------------------------------------------
UINT Render24(UINT blocksize, UINT channels, const FLAC__int32* const data[], void* dest)
{
	Int4Byte ib;
	UINT i = 0;

	BYTE* outbuf = static_cast<BYTE*>(dest);
	for (UINT s = 0; s < blocksize; ++s) {
		for (UINT ch = 0; ch < channels; ++ch) {
			ib.i = data[ch][s];
			outbuf[i++] = ib.b[0];
			outbuf[i++] = ib.b[1];
			outbuf[i++] = ib.b[2];
		}
	}

	return i;
}

//-----------------------------------------------------------------------------
// 32bitレンダリング
//-----------------------------------------------------------------------------
UINT Render32(UINT blocksize, UINT channels, const FLAC__int32* const data[], void* dest)
{
	UINT i = 0;

	int* outbuf = static_cast<int*>(dest);
	for (UINT s = 0; s < blocksize; ++s) {
		for (UINT ch = 0; ch < channels; ++ch) {
			outbuf[i++] = data[ch][s];
		}
	}

	return i * sizeof(int);
}
