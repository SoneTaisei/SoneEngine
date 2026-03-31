#include "AudioManager.h"
#include <cassert>
#include "Core/Utility/Utilityfunctions.h"

// 静的メンバ変数の実体を定義
Microsoft::WRL::ComPtr<IXAudio2> AudioManager::xAudio2_ = nullptr;
std::unique_ptr<IXAudio2MasteringVoice, MasteringVoiceDeleter> AudioManager::masterVoice_ = nullptr;
std::list<std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter>> AudioManager::playingVoices_;
std::map<std::string, SoundData> AudioManager::soundDatas_;

void AudioManager::Initialize() {
	HRESULT result;
	// XAudioエンジンのインスタンスを生成
	result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	// マスターボイスの生成
    // 1. まずは一時的な「生ポインタ」を用意する
    IXAudio2MasteringVoice *pMasterVoiceRaw = nullptr;

    // 2. 生成関数にそのポインタを渡す
    result = xAudio2_->CreateMasteringVoice(&pMasterVoiceRaw);
    assert(SUCCEEDED(result));

    // 3. 成功したら、すぐに unique_ptr に所有権を渡す（管理を任せる）
    masterVoice_.reset(pMasterVoiceRaw);

	// Media Foundationの初期化
    result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    assert(SUCCEEDED(result));
}

void AudioManager::Finalize() {
    // 1. 再生中のボイスをすべて消す
    playingVoices_.clear();

    // 2. 読み込んだサウンドデータを解放
    for (auto &pair : soundDatas_) {
        SoundUnload(&pair.second);
    }
    soundDatas_.clear();

    // 3. ★【重要】マスターボイスをエンジンより先に消す！
    // unique_ptrの自動解放を待つと、下のxAudio2_.Reset()の後になってしまいクラッシュします。
    masterVoice_.reset();

    // 4. Media Foundationの終了処理
    MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET); // Initializeにあったので念のため
    MFShutdown();

    // 5. 最後にエンジンを解放する
    xAudio2_.Reset();
}

const std::string &AudioManager::LoadSound(const std::string &filename) {
	// すでに読み込み済みか検索
	auto it = soundDatas_.find(filename);
	// 読み込み済みなら、そのキー（ファイルパス）を返す
	if(it != soundDatas_.end()) {
		return it->first;
	}
	// WAVファイルを読み込む
    SoundData soundData = SoundLoadMediaFoundation(filename.c_str());
	// 読み込みに失敗していないかチェック
	assert(soundData.pBuffer && "Failed to load sound file.");
	// 読み込んだデータをマップに格納
	soundDatas_[filename] = std::move(soundData);
	// 格納したデータのキーを返す
	return soundDatas_.find(filename)->first;
}

void AudioManager::Play(const std::string &filename) {
	// ファイル名を元にサウンドデータを検索
	auto it = soundDatas_.find(filename);
	// データが見つからなければアサートして中断
	if(it == soundDatas_.end()) {
		assert(!"AudioManager::Play() : Sound data not found. Please LoadSound() first.");
		return;
	}
		// 見つかったサウンドデータを参照
	const SoundData &soundData = it->second;
	HRESULT result;

	// 1. SourceVoiceを生成
	IXAudio2SourceVoice *pSourceVoiceRaw = nullptr;
	result = xAudio2_->CreateSourceVoice(&pSourceVoiceRaw, &soundData.wfex);
	assert(SUCCEEDED(result));

	// 2. 作成したボイスをカスタムデリータ付きのunique_ptrでラップ
	std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter> pSourceVoice(pSourceVoiceRaw);

	// 3. 再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer.get();
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	// 4. 波形データを登録して再生開始
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	assert(SUCCEEDED(result));
	result = pSourceVoice->Start();
	assert(SUCCEEDED(result));

	// 5. 所有権をリストに移動して管理対象にする
	playingVoices_.push_back(std::move(pSourceVoice));
}

void AudioManager::Update() {
	// 再生が終わったボイスをリストから削除する
	playingVoices_.remove_if([](const auto &voice) {
		XAUDIO2_VOICE_STATE state{};
		voice->GetState(&state);
		// バッファのキューが空になったら再生終了とみなす
		return state.BuffersQueued == 0;
							 });
}