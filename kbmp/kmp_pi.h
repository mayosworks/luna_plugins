#ifndef KMP_PI_H
#define KMP_PI_H

/*

   KbMedia Player Plugin SDK (2015/06/14 ��)

   �J���Ҍ����ł��B
   KbMedia Player �̃v���O�C�����J�����Ă݂����l���������ǂ݉������B
   �蔲���ŃR�����g�� C++ �p�ɂȂ��Ă܂����A�����̃R���p�C���Ȃ� C �ł����v�ł��傤�B

   ���ۂ̃v���O�C���̃\�[�X�R�[�h��

   http://hp.vector.co.jp/authors/VA015058/source/

   �ɂ���܂��B

   �K�v�ŏ����̋@�\�����Ȃ����߁A�o���邱�Ƃ͂��Ȃ萧������܂����A
   ���̕��v���O�C�������̂͊ȒP�Ȃ͂��ł��B�v���O�C�����g���̂��ȒP�ł��傤�B

   ���ŗ���

   �E2015/06/14
     �Ekmp_GetTestTagInfo ��ǉ�
     �E�ʎq���r�b�g���� 64bit float ��ǉ�

*/

#define KMPMODULE_VERSION 100 //KMPMODULE �̃o�[�W����
#define SZ_KMP_GETMODULE  "kmp_GetTestModule" //�܂��i���܂ŁH�j�e�X�g��...
#define SZ_KMP_CONFIG     "kmp_Config"        //�v���O�C���ݒ�p�iv2.38beta2 �ȍ~�őΉ��j
#define SZ_KMP_GETTAGINFO "kmp_GetTestTagInfo"//�^�O�擾�p�iv2.51beta3�ȍ~�őΉ��j

typedef void* HKMP;//'K'b'M'edia 'P'layer Plugin �� Handle

typedef struct
{//�I�[�v�������T�E���h�f�[�^�̏��
    DWORD dwSamplesPerSec;//�T���v�����O���g��(44100, 22050 �Ȃ�)
    DWORD dwChannels;     //�`�����l����( mono = 1, stereo = 2)
    DWORD dwBitsPerSample;//�ʎq���r�b�g��( 8 or 16 or 24 or 32 or -32(32bit float) or -64(64bit float) )
                          //�{�̂���̃r�b�g���v���Ɋւ�炸�Ή��o����ő�̃r�b�g����Ԃ����Ƃ𐄏�
    DWORD dwLength;       //�Ȃ̒����i�v�Z�s�\�ȏꍇ�� 0xFFFFFFFF�j
                          //�P�t�@�C�������ȁidwReserved2 == 1�j�̂Ƃ��́A�Ȑ��� 1000 �{�����l�ɂ���B
                          //�Ⴆ�� 10 �Ȃ̏ꍇ�� dwLength == 10000 �Ƃ���B
                          //�P�t�@�C�������Ȃ̏ꍇ�A�Ȗ��̉��t���Ԃ��擾���邱�Ƃ͏o���Ȃ��B
                          //�ʖڂȎd�l�ŃX�~�}�Z��
    DWORD dwSeekable;     //�V�[�N�ɑΉ����Ă���ꍇ�� 1�A���Ȃ��ꍇ�� 0
    DWORD dwUnitRender;   //Render �֐��̑�R�����͂��̒l���n�����i�ǂ�Ȓl�ł��ǂ��ꍇ�� 0�j
    DWORD dwReserved1;    //�������[�v���ĉ��t�I�����Ȃ��Ȃ̏ꍇ�� 1, �����łȂ��ꍇ�� 0
                          //Render �̌Ăяo�����J��Ԃ��Ă����t�I�����Ȃ��ꍇ�� 1 �ɂ���
                          //dwReserved1 == 1 �̏ꍇ�A
                          //  �E�v���C���[�{�̂́u�P�ȃ��s�[�g�v���L���ɂȂ��Ă���ꍇ�͖������[�v
                          //  �EdwLength != 0xFFFFFFFF �̏ꍇ�� dwLength �����Đ�������Ƀv���C���[
                          //    �{�̂ɂ���ăt�F�[�h�A�E�g�E���t�I�������
                          //  �EdwLength == 0xFFFFFFFF �̏ꍇ�̓v���C���[�̐ݒ�́u�f�t�H���g�̒����v
                          //    �����Đ�������Ƀt�F�[�h�A�E�g�E���t�I�������
                          //dwReserved1 == 0 �̏ꍇ�A
                          //  �ERender �̖߂�l����R����(dwSize)��菬�����l���Ԃ�܂ōĐ������
    DWORD dwReserved2;    //�P�t�@�C�������Ȃ̏ꍇ�� 1, �P�t�@�C���P�Ȃ̂Ƃ��� 0
                          //dwReserved2 == 1 �̏ꍇ�AdwLength �ɂ͋Ȑ��� 1000 �{�����l��Ԃ����ƁB
                          //dwReserved2 == 1 �̏ꍇ�ASetPosition �� dwPos ��(�Ȕԍ� * 1000)���n�����
                          //�悤�ɂȂ�A�V�[�N�͏o���Ȃ��Ȃ�̂� dwSeek == 0 �Ƃ��邱�ƁB
                          //���݂̎d�l�ł́A�P�t�@�C�������Ȃ̊g���q�� KbMedia Player �{�̂Ō��ߑł�
                          //����Ă���A�{�̂��m��Ȃ��g���q�ɑ΂��Ă��̒l�� 1 �ɂ��Ă�����ɂ͓���
                          //���Ȃ����A�{�̂��P�t�@�C�������ȂƂ��Ĉ����Ă���`���ɑ΂��Ă��̒l�� 0 ��
                          //���Ă��A��͂萳�퓮�삵�Ȃ��i����
                          //�ʖڂȎd�l�ŃX�~�}�Z��
}SOUNDINFO;


typedef struct
{
    DWORD dwVersion;
    //���W���[���̃o�[�W�����B�v���O�C���̃o�[�W�����ł͂Ȃ��B
    //�K�� KMPMODULE_VERSION(=100) �ɂ��邱�ƁB
    //���̒l�� KbMedia Player �����҂���l�ƈ�v���Ȃ��ꍇ�́AKbMedia Player
    //�ɂ���Ē����� FreeLibrary ���Ă΂��B
    //���̏ꍇ�AInit() �� Deinit() ���Ă΂�Ȃ����Ƃɒ��ӁB

    DWORD dwPluginVersion;
    //�v���O�C���̃o�[�W����
    //�Ή��g���q����������v���O�C�������������ꍇ�́A�������傫�����̂�D��I�Ɏg��

    const char  *pszCopyright;
    //���쌠
    //�o�[�W�������ł��̕����̕������\������
    //NULL �ɂ��Ă��悢

    const char  *pszDescription;
    //����
    //�o�[�W�������ł��̕����̕������\������
    //NULL �ɂ��Ă��悢

    const char  **ppszSupportExts;
    //�Ή��g���q�̕�����̔z��(�s���I�h�܂�)
    //NULL �ŏI���悤�ɂ���
    //��FppszSupportExts = {".mp1", ".mp2", ".mp3", "rmp", NULL};
    //
    //ppszSupportExts == {NULL} (ppszSupportExts == NULL �͕s�j�ŁA���� Init �� Deinit
    //�� �� NULL �̏ꍇ�͏풓�v���O�C���Ƃ��Ĉ����AKbMedia Player �����쒆�͏�Ƀ��[�h
    //���ꂽ�܂܂ɂȂ�i������̋@�\�j
    //�풓�v���O�C���Ƃ��Ă̓���� Init �ďo�����ɊJ�n���ADeinit �ďo�����ɏI�����邱�ƁB
    //�iLoadLibrary ���ɓ���J�n�AFreeLibrary ���ɓ���I���͕s�j
    //
    //��L�ȊO�őΉ��g���q���P���Ȃ��ꍇ�A�s���ȃv���O�C���ƌ��Ȃ���AKbMedia Player
    //�ɂ���Ē����� FreeLibrary �����B
    //���̏ꍇ�� Init() �� Deinit() ���Ă΂�Ȃ����Ƃɒ��ӁB

    DWORD dwReentrant;
    //�����t�@�C���̓����Đ����\�ȏꍇ�� 1, �s�\�ȏꍇ�� 0
    //Open �̖߂�l�� HKMP ���Q�ȏ㓯���Ɏ��ĂȂ��ꍇ�� 0 �ɂ���
    //���ʂȎ���Ȃ�����A�o���邾�� dwReentrant == 1 �ƂȂ�悤�Ɏ�������ׂ��ł���
    //���̒l�� 0 �̏ꍇ�A�Q�ڈȍ~�̃t�@�C���̓v���C���[�{�̂Ƃ͕� EXE ���o�R���ĊJ��
    //�悤�ɂȂ�B
    //���炩�̗��R�ɂ��A��Ƀv���C���[�{�̂Ƃ͕� EXE �Ńt�@�C�����J���悤�ɂ������ꍇ
    //�� dwReentrant == 0xFFFFFFFF �Ƃ��邱��
    //�Ⴆ�΁Aaaa.kpi �� bbb.kpi �� ccc.dll �Ɉˑ����Accc.dll ���m�����G���g�����g�ł���
    //�ꍇ�́Aaaa.kpi �� bbb.kpi �� dwReentrant == 0xFFFFFFFF �ɂȂ��Ă��������]�܂����B

    void  (WINAPI *Init)(void);
    //�v���O�C���������BOpen �����Ăяo���O�� KbMedia Player �ɂ���Ĉ�x�����Ă΂��B
    //�K�v�Ȃ��ꍇ�� NULL �ɂ��Ă��ǂ��B
    //�������A�풓�v���O�C���i������̋@�\�j�̏ꍇ�͕K���������邱�ƁB
    //Init �̌Ăяo���ŏ풓�v���O�C���𓮍�J�n����B
    //LoadLibrary ���ɏ풓�v���O�C���𓮍�J�n���Ă͂����Ȃ��B

    void  (WINAPI *Deinit)(void);
    //�v���O�C���̌�n���BFreeLibrary �̒��O�Ɉ�x�����Ă΂��B
    //Init() ����x���Ă΂��� Deinit() ���Ăԉ\�������邱�Ƃɒ��ӁB
    //�K�v�Ȃ��ꍇ�� NULL �ɂ��Ă��ǂ��B
    //�������A�풓�v���O�C���i������̋@�\�j�̏ꍇ�͕K���������邱�ƁB
    //Deinit �̌Ăяo���ŏ풓�v���O�C���𓮍�I������
    //FreeLibrary ���ɏ풓�v���O�C���𓮍�I�����Ă͂����Ȃ��B

    HKMP (WINAPI *Open)(const char *cszFileName, SOUNDINFO *pInfo);
    //�t�@�C�����J���B�K���������邱�ƁB
    //�G���[�̏ꍇ�� NULL ��Ԃ��B
    //�G���[�łȂ��ꍇ�� pInfo �ɓK�؂ȏ������邱�ƁB�K�؂ȏ�񂪓�����
    //���Ȃ��ꍇ�idwBitsPerSample �� 0 �Ȃǁj�� KbMedia Player �ɂ���Ē����� Close
    //���Ă΂��B
    //Open �ɓn���ꂽ pInfo->dwSampleRate/dwBitsPerSample/dwChannels �̊e�l�� 0 �ȊO��
    //�ꍇ�A�\�Ȃ炻��ɏ]�����`���i�܂��͍ł��߂��`���j�Ńt�@�C�����J���悤�ɂ��邱�ƁB
    //�i�Ⴆ�� pInfo->dwBitsPerSample == -64(64bit float) �ɂȂ��Ă���ꍇ�A�\�Ȃ�
    //  64bit float �ŁA64bit float ���s�\�ł� 32bit int �Ȃ�Ή����Ă���Ȃ� 32bit int
    //  �ŕԂ��Ȃǁj
    //�ʎq���r�b�g���� 2.51beta15 �ȍ~�͏�� 64bit float �ŗv��������悤�ɂȂ��Ă���B
    //�Ή��o����͈͓��ōő�̂��̂�Ԃ��悤�ɂ��邱�ƁB�\�Ȃ�N���b�s���O�̏������s�v��
    //64bit float �� 32bit float ���]�܂����B
    //�{�̂͏�ɓ����ň�U 64bit float �ɕϊ�����B64bit float �ŏo�͏o����̂ł����
    //�ϊ��̕K�v���Ȃ��Ȃ�A�ł������I�ƂȂ�B
    //������ƌ����Ė������� 64bit float �ł̏o�͂ɑΉ�����K�v�͂Ȃ��A�Ή��o����ő��
    //�r�b�g�����ǂꂩ�P�����ɍi�荞�߂Ηǂ�
    //�����̃r�b�g���ɑΉ���������v���O�C���J�����̎�Ԃ����Ȃ����ł���B

    HKMP (WINAPI *OpenFromBuffer)(const BYTE *Buffer, DWORD dwSize, SOUNDINFO *pInfo);
    //���g�p�BNULL �ɂ��邱�ƁB�� NULL �ł��G���[�ɂ͂Ȃ�Ȃ����A�g�p����Ȃ��B

    void   (WINAPI *Close)(HKMP hKMP);
    //�n���h�������B�K���������邱�ƁB

    DWORD  (WINAPI *Render)(HKMP hKMP, BYTE* Buffer, DWORD dwSize);
    //Buffer �� PCM ������B�K���������邱�ƁB
    //dwSize �� Buffer �̃T�C�Y�̃o�C�g���B�i�T���v�����ł͂Ȃ��j
    //�߂�l�� Buffer �ɏ������񂾃o�C�g���B�i�T���v�����ł͂Ȃ��j
    //dwSize ��菬�����l��Ԃ����牉�t�I���B
    //dwSize �� SOUNDINFO::dwUnitRender �̒l���n�����B
    //SOUNDINFO::dwUnitRender == 0 �̏ꍇ�� dwSize �ɂ͔C�ӂ̒l���n�����B
    //�idwSize �̓T���v��������̃o�C�g���̔{���ł��邱�Ƃ��ۏ؂���Ă���j

    DWORD  (WINAPI *SetPosition)(HKMP hKMP, DWORD dwPos);
    //�V�[�N�B�K���������邱�ƁB
    //dwPos �̓V�[�N��̍Đ��ʒu�B�߂�l�̓V�[�N��̍Đ��ʒu�B�P�ʂ̓~���b�B
    //dwPos �Ɩ߂�l�͊��S�Ɉ�v����K�v�͂Ȃ��B�߂�l�Ɩ{���̍Đ��ʒu��
    //�덷���傫���Ȃ�i�̎��Ƃ̓����Đ����Ɏx����������j�ꍇ�� Open ����
    //SOUNDINFO �� dwSeekable �� 0 �ɂ��Ă������ƁB�덷���Ȃ����A�����Ă�
    //���ɏ������ꍇ�� dwSeekable �� 1 �ɂ��Ă������ƁB�߂�l�����m�Ȃ�
    //�΁AdwPos �Ɩ߂�l�̍����傫���Ă� dwSeekable=1 �Ƃ��ėǂ��B
    //�������A���ۂ̃V�[�N��ʒu�� dwPos ������ɂȂ邱�Ƃ͔����邱�ƁB
    //�iCueSheet �Đ����ɖ�肪�N����₷���j
    //
    //�V�[�N�ɑS���Ή����Ȃ��ꍇ�́A�擪�ʒu�ɖ߂��� 0 ��Ԃ����ƁB
    //
    //�P�t�@�C�������ȁiSOUNDINFO::dwReserved2 == 1�j�̏ꍇ�A���̊֐��őI��
    //���邱�ƂɂȂ�B�Ȕԍ��� 1 �x�[�X�i�Ȕԍ� 0 �͑��݂��Ȃ��j�ŁAdwPos
    //�ɂ́A�Ȕԍ��� 1000 �{�����l������B�Ⴆ�� 5 �Ԗڂ̋Ȃɐ؂�ւ���ꍇ
    //�� dwPos == 5000 �ƂȂ�B
    //�؂�ւ���̍Đ��ʒu�͐擪�ɖ߂��� 0 ��Ԃ����ƁB
    //�������AdwPos == 0 �̏ꍇ�͋Ȃ�؂�ւ����ɍĐ��ʒu��擪�ɖ߂����ƁB
    //SetPosition ����x���Ă΂�Ă��Ȃ���Ԃł� 1 �Ȗڂ��I������Ă���悤��
    //���邱�ƁB
    //�؂�ւ��Ɠ����ɃV�[�N���邱�Ƃ͏o���Ȃ��B�i�ʖڂȎd�l�ŃX�~�}�Z���j

}KMPMODULE;

typedef KMPMODULE* (WINAPI *pfnGetKMPModule)(void);
//KMPMODULE* WINAPI kmp_GetTestModule(void);//�G�N�X�|�[�g����֐���
//
//KMPMODULE �\���̂ւ̃|�C���^��Ԃ��B�K���G�N�X�|�[�g���邱�ƁB
//���̊֐��̓v���O�C���� LoadLibrary ����Ă��� FreeLibrary �����܂ł̊Ԃ�
//KbMedia Player �{�̂���͂P�񂾂������Ă΂�Ȃ��B
//���̊֐����Ԃ��� KMPMODULE �\���̂̃f�[�^�����o�́AFreeLibrary �����܂�
//�ύX���Ă͂����Ȃ��B
//�]���āA�Ⴆ�ΑΉ��g���q��ݒ�ɂ���ĕύX�����悤�ɂ������ꍇ�A�Ή��g���q
//�̕ύX�� KbMedia Player �{�̂ɔF�������̂́A�v���O�C���ă��[�h���Ƃ�������
//�ɂȂ�B

typedef DWORD (WINAPI *pfnKmpConfig)(HWND hWnd, DWORD dwVersion, DWORD dwReserved);
//DWORD WINAPI kmp_Config(HWND hWnd, DWORD dwVersion, DWORD dwReserved);//�G�N�X�|�[�g����֐���
//
//�v���O�C���̐ݒ���s���B���ɐݒ�̕K�v���Ȃ��ꍇ�̓G�N�X�|�[�g���Ȃ��Ă��ǂ��B
//   �EdwVersion/dwReserved �Ƃ��� 0 ���n�����
//   �E�߂�l�͌���ł� 0 ��Ԃ��̂�
//   �EHKMP �̃C���X�^���X���Ƃ̐ݒ�͌���ł͖��Ή��i�Ή��̕K�v������̂��H�j
//   �Ekmp_Config ���Ă΂��O�� KMPMODULE::Init() ���Ă΂��
//   �Ekmp_Config ����߂��Ă��Ȃ��i�ݒ�_�C�A���O��\�����Ă���j��Ԃł́A
//     �E��d�� kmp_Config ���Ă΂�邱�Ƃ͂Ȃ�
//     �EKMPMODULE::Deinit() �͌Ă΂�Ȃ�
//     �EFreeLibrary �͌Ă΂�Ȃ�
//     �Ekmp_Config �Ăяo����A�_�C�A���O��\�������܂ܒ����Ƀ��^�[������ꍇ��
//       ���̌���ł͂Ȃ��i���̏ꍇ�̓v���O�C�����œK�؂ɏ������邱�Ɓj
//
//�ݒ�͂������� kmp_Config �̎������ʓ|�A�Ƃ����ꍇ�́A�v���O�C���Ɠ����ꏊ��
//�v���O�C���Ɠ����� INI �t�@�C����u���΁AKbMedia Player �{�̂̃v���O�C���ݒ�
//�_�C�A���O���� INI �t�@�C�����J���� INI �t�@�C���𒼐ڕҏW�o����悤�ɂȂ��Ă���B

//�^�O�擾�p
#define SZ_KMP_TAGINFO_NAME_TITLE       "title"    //�Ȗ�
#define SZ_KMP_TAGINFO_NAME_ARTIST      "artist"   //�A�[�e�B�X�g
#define SZ_KMP_TAGINFO_NAME_ALBUM       "album"    //�A���o��
#define SZ_KMP_TAGINFO_NAME_GENRE       "genre"    //�W������
#define SZ_KMP_TAGINFO_NAME_DATE        "date"     //���t
#define SZ_KMP_TAGINFO_NAME_COMMENT     "comment"  //�R�����g
#define SZ_KMP_TAGINFO_NAME_ALBUMARTIST "albumartist" //�A���o���A�[�e�B�X�g
#define SZ_KMP_TAGINFO_NAME_BITRATE     "bitrate"     //�r�b�g���[�g
#define SZ_KMP_TAGINFO_NAME_TRACKNUMBER "tracknumber" //�g���b�N�ԍ�
#define SZ_KMP_TAGINFO_NAME_DISCNUMBER  "discnumber"  //�f�B�X�N�ԍ�
#define SZ_KMP_TAGINFO_NAME_TRACKTOTAL  "tracktotal"  //�g���b�N��
#define SZ_KMP_TAGINFO_NAME_DISCTOTAL   "disctotal"   //�f�B�X�N��
#define SZ_KMP_TAGINFO_NAME_REPLAYGAIN_TRACKGAIN "replaygain_track_gain" //���v���C�Q�C��
#define SZ_KMP_TAGINFO_NAME_REPLAYGAIN_TRACKPEAK "replaygain_track_peak" //���v���C�Q�C��
#define SZ_KMP_TAGINFO_NAME_REPLAYGAIN_ALBUMGAIN "replaygain_album_gain" //���v���C�Q�C��
#define SZ_KMP_TAGINFO_NAME_REPLAYGAIN_ALBUMPEAK "replaygain_album_peak" //���v���C�Q�C��

class IKmpTagInfo
{//��������͖̂{�̑�
 //�v���O�C�����͌Ăяo������
public:
    virtual void WINAPI SetValueA(const char *cszName, const char *cszValue) = 0;   //Ansi
    virtual void WINAPI SetValueW(const char *cszName, const WCHAR *cwszValue) = 0; //Unicode
    virtual void WINAPI SetValueU8(const char *cszName, const char *cszValueU8) = 0;//Utf8
};
typedef BOOL (WINAPI *pfnGetTagInfo)(const char *cszFileName, IKmpTagInfo *pInfo);
//DWORD WINAPI kmp_GetTestTagInfo(const char *cszFileName, IKmpTagInfo *pInfo);//�G�N�X�|�[�g����֐���
//
//�^�C�g�����̃^�O����r�b�g���[�g���̏��� IKmpTagInfo �ɒʒm����
//�G�N�X�|�[�g����Ă��Ȃ��ꍇ�͖{�̂̃^�O�擾���[�`���Ń^�O�����擾����
//
//���̂悤�Ɏ�������
//BOOL WINAPI kmp_GetTestTagInfo(const char *cszFileName, IKmpTagInfo *pInfo)
//{
//    char  *szTitle;   //Ansi
//    WCHAR *wszArtist; //Unicode
//    char  *szU8Album; //Utf8
//    ...
//    pInfo->SetValueA(SZ_KMP_TAGINFO_NAME_TITLE, szTitle);
//    pInfo->SetValueW(SZ_KMP_TAGINFO_NAME_ARTIST, szArtist);
//    pInfo->SetValueU8(SZ_KMP_TAGINFO_NAME_ALBUM, szU8Album);
//    pInfo->SetValueA(SZ_KMP_TAGINFO_NAME_BITRATE, "128kbps");
//    ...
//    return TRUE;
//}
//
//SetValueA/W/U8 �͂��ꂼ�� Ansi/Unicode/Utf8
//
//cszName �͑啶���Ə���������ʂ��Ȃ�
//cszName �ň�����̂� KMP_TAGINFO_NAME_XXXX
//SZ_KMP_TAGINFO_NAME_XXXX �Ŗ���`�̕������n���Ă��{�̂ɂ���Ė��������B
//��`�ς݂ł��{�̂̃o�[�W�����ɂ���Ă͖�������邱�Ƃ�����B
//
//SetValueA(ANSI) �� SetValueW(UNICODE) �� SetValueU8(UTF8) �͍��݂��Ďg�p�\
//3 ��ޗp�ӂ����̂̓v���O�C�����ł̕����R�[�h�̕ϊ��̎�Ԃ��Ȃ�����
//���� Value �l���킴�킴 3 ��ނ̕����R�[�h�Ŏw�肵�ČĂԕK�v�͂Ȃ�
//    pInfo->SetValueA(SZ_KMP_TAGINFO_NAME_TITLE, "�����Ƃ�");
//    pInfo->SetValueW(SZ_KMP_TAGINFO_NAME_TITLE, L"�����Ƃ�");
//    pInfo->SetValueU8(SZ_KMP_TAGINFO_NAME_TITLE, (utf8��)"�����Ƃ�");
//�̂悤�ȌĂѕ��͕s�v
//cszName �͏�� ANSI �ł��邱�Ƃɒ���(�}���`�o�C�g�����͎g���Ȃ�)
//
//KbMedia Player �́A�ȉ��̏����Ń^�O�����擾����i�ꕔ�������j
//�i�����I�ɂ͈ȉ��̂悤�ɂ���\�肾���������j
//
//1.���ۂ̍Đ��Ɏg���v���O�C������擾
//2.�����`���ɑΉ�����ʂ̃v���O�C������擾(�g�p�D��x��������)
//3.KbMedia Player ���g���擾
//1 > 2 > 3 �̏��ɗD��x������(1���ł��D��x������)
//
//�Ⴆ�΃r�b�g���[�g�ȊO�̏��擾�͑��̃v���O�C���Ȃ� KbMedia Player ���g��
//�C��������΁A�r�b�g���[�g��񂾂���ʒm����Ηǂ��B
//
//�t�ɑS�Ă̏��擾�����g�ōs���A2, 3 �̎擾���ȗ����������ꍇ�́A
//��ԍŏ��� SetValueA(NULL, NULL) ���Ăяo��(SetValueW/SetValueU8 �ł���)
//��F
//    pInfo->SetValueA(NULL, NULL);//SetValueW(NULL, NULL)/SetValueU8(NULL, NULL)�� OK
//    pInfo->SetValueA("bitrate", "128kbps");
//    pInfo->SetValueA("title", "�^�C�g��");
//���̏ꍇ�A2,3 �ŃA�[�e�B�X�g��񓙂��擾�o���Ă��A1 �Ŏ擾������񂵂��\�����Ȃ��B
//
//SetValue(NULL, NULL) ���Ȃ��ꍇ�AcszName ���w�肳��ĂȂ����̂Ɋւ��Ă� 2,3 �Ŏ擾
//�o����Ε\������B��L�̗�ł́A�A�[�e�B�X�g���� 2,3 �œ����ꍇ�̓A�[�e�B�X�g���
//���\������B
//
//1,2,3 �œ��� cszName �ɑ΂��ĈقȂ� cszValue ������ꂽ�ꍇ�A�D��x���������̂��̗p����
//�]���āA
//    pInfo->SetValueA("title", "");
//�̂悤�ɌĂяo���ƁA2,3 �ňقȂ�^�C�g���𓾂Ă��^�C�g���� "" �ƂȂ�B
//
#endif
