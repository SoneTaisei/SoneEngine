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

    // 音声再生（ハンドルを使って再生）
    static void Play(const std::string & filename);

    // 毎フレームの更新処理（再生が終わったボイスを破棄する）
    static void Update();

private:
    // XAudio2の本体など
    static Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    static std::unique_ptr<IXAudio2MasteringVoice, MasteringVoiceDeleter> masterVoice_;

    // 再生中のボイスを管理するリスト
    static std::list<std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter>> playingVoices_;

    // 読み込んだ音声データを管理するマップ
    static std::map<std::string, SoundData> soundDatas_;
};