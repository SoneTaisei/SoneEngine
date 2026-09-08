#pragma once
#include "Core/Utility/Structs.h" // SoundDataなどを使うために必要
#include <list>
#include <memory> // std::unique_ptr を使うために必要
#include <string>
#include <map>

// IXAudio2SourceVoiceを自動で破棄するためのカスタムデリータ
struct SourceVoiceDeleter {
    void operator()(IXAudio2SourceVoice *p) const {
        if(p) {
            // Release()ではなくDestroyVoice()を呼ぶ
            p->DestroyVoice();
        }
    }
};

struct MasteringVoiceDeleter {
    void operator()(IXAudio2MasteringVoice *p) const {
        if (p)
            p->DestroyVoice(); // MasteringVoiceもDestroyVoiceで消す
    }
};

class AudioManager {
public:
    // 初期化
    static void Initialize();
    // 終了処理
    static void Finalize();


    // 音声データを読み込む（ハンドルとしてファイルパスを返す）
    static const std::string & LoadSound(const std::string & filename);

    // 音声再生（SEなど単発・効果音向け）
    static void Play(const std::string & filename, float volume = 1.0f, bool loop = false);

    // BGM再生（指定した曲をループまたは単発で再生。すでに再生中の場合は何もしない）
    static void PlayBGM(const std::string & filename, bool loop = true, float volume = 0.5f);

    // 特定のBGMを停止
    static void StopBGM(const std::string & filename);

    // 再生中のすべてのBGMを停止
    static void StopAllBGM();

    // 特定のBGMの一時停止
    static void PauseBGM(const std::string & filename);

    // 特定のBGMの再開
    static void ResumeBGM(const std::string & filename);

    // 特定のBGMの音量設定 (0.0f - 1.0f)
    static void SetBGMVolume(const std::string & filename, float volume);

    // 特定のBGMの音量取得
    static float GetBGMVolume(const std::string & filename);

    // 特定のBGMが再生中か確認
    static bool IsBGMPlaying(const std::string & filename);

    // BGM再生許可フラグの設定（エディター停止中のミュート制御用）
    static void SetBGMPlaybackAllowed(bool allowed);
    static bool IsBGMPlaybackAllowed();

    // 毎フレームの更新処理（再生が終わったボイスを破棄する）
    static void Update();

private:
    struct BGMRequest {
        bool loop = true;
        float volume = 0.5f;
    };

    // 内部ボイス生成・破棄補助
    static void StartBGMVoice(const std::string & filename, bool loop, float volume);
    static void StopBGMVoice(const std::string & filename);

    // XAudio2の本体など
    static Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    static std::unique_ptr<IXAudio2MasteringVoice, MasteringVoiceDeleter> masterVoice_;

    // 再生中のボイスを管理するリスト（SE用）
    static std::list<std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter>> playingVoices_;

    // 再生中のBGMボイスを管理するマップ（曲ごとに独立して管理、複数同時再生に対応）
    static std::map<std::string, std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter>> bgmVoices_;

    // シーン等からリクエストされたBGM（エディタ停止中などでも保持し、PLAY時に即時再生するための情報）
    static std::map<std::string, BGMRequest> requestedBGMs_;

    // BGMの再生許可フラグ（エディター停止時はfalse）
    static bool isBgmPlaybackAllowed_;

    // 読み込んだ音声データを管理するマップ
    static std::map<std::string, SoundData> soundDatas_;
};