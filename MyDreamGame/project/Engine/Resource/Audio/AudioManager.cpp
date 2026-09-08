#include "AudioManager.h"
#include <cassert>
#include "Core/Utility/Utilityfunctions.h"

// 静的メンバ変数の実体を定義
Microsoft::WRL::ComPtr<IXAudio2> AudioManager::xAudio2_ = nullptr;
std::unique_ptr<IXAudio2MasteringVoice, MasteringVoiceDeleter> AudioManager::masterVoice_ = nullptr;
std::list<std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter>> AudioManager::playingVoices_;
std::map<std::string, std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter>> AudioManager::bgmVoices_;
std::map<std::string, AudioManager::BGMRequest> AudioManager::requestedBGMs_;
bool AudioManager::isBgmPlaybackAllowed_ = true;
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
    // 1. 再生中のBGMをすべて停止・消去
    StopAllBGM();

    // 2. 再生中のSEボイスをすべて消す
    playingVoices_.clear();

    // 3. 読み込んだサウンドデータを解放
    for (auto &pair : soundDatas_) {
        SoundUnload(&pair.second);
    }
    soundDatas_.clear();

    // 4. ★【重要】マスターボイスをエンジンより先に消す！
    // unique_ptrの自動解放を待つと、下のxAudio2_.Reset()の後になってしまいクラッシュします。
    masterVoice_.reset();

    // 5. Media Foundationの終了処理
    MFShutdown();

    // 6. 最後にエンジンを解放する
    xAudio2_.Reset();
}

const std::string &AudioManager::LoadSound(const std::string &filename) {
	// すでに読み込み済みか検索
	auto it = soundDatas_.find(filename);
	// 読み込み済みなら、そのキー（ファイルパス）を返す
	if(it != soundDatas_.end()) {
		return it->first;
	}
	// サウンドファイルを読み込む (MP3, WAV等対応)
    SoundData soundData = SoundLoadMediaFoundation(filename.c_str());
	// 読み込みに失敗していないかチェック
	assert(soundData.pBuffer && "Failed to load sound file.");
	// 読み込んだデータをマップに格納
	soundDatas_[filename] = std::move(soundData);
	// 格納したデータのキーを返す
	return soundDatas_.find(filename)->first;
}

void AudioManager::Play(const std::string &filename, float volume, bool loop) {
	// 事前にロードされていなければロード
	LoadSound(filename);

	auto it = soundDatas_.find(filename);
	if(it == soundDatas_.end()) {
		assert(!"AudioManager::Play() : Sound data not found.");
		return;
	}

	const SoundData &soundData = it->second;
	HRESULT result;

	// 1. SourceVoiceを生成
	IXAudio2SourceVoice *pSourceVoiceRaw = nullptr;
	result = xAudio2_->CreateSourceVoice(&pSourceVoiceRaw, &soundData.wfex);
	if (FAILED(result)) {
		return;
	}

	std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter> pSourceVoice(pSourceVoiceRaw);

	// 2. 再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer.get();
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;
	buf.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

	// 3. 波形データを登録して音量設定・再生開始
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	if (FAILED(result)) {
		return;
	}
	pSourceVoice->SetVolume(volume);
	result = pSourceVoice->Start();
	if (FAILED(result)) {
		return;
	}

	// 4. 所有権をリストに移動して管理対象にする
	playingVoices_.push_back(std::move(pSourceVoice));
}

void AudioManager::StartBGMVoice(const std::string &filename, bool loop, float volume) {
	// すでにこのBGMが再生中であれば、多重再生・頭出しを防止して音量更新のみ
	auto itBgm = bgmVoices_.find(filename);
	if (itBgm != bgmVoices_.end() && itBgm->second) {
		XAUDIO2_VOICE_STATE state{};
		itBgm->second->GetState(&state);
		if (state.BuffersQueued > 0) {
			itBgm->second->SetVolume(volume);
			return;
		}
		itBgm->second->Stop();
		bgmVoices_.erase(itBgm);
	}

	// 事前にロード
	LoadSound(filename);

	auto it = soundDatas_.find(filename);
	if (it == soundDatas_.end()) {
		assert(!"AudioManager::StartBGMVoice() : Sound data not found.");
		return;
	}

	const SoundData &soundData = it->second;
	HRESULT result;

	// SourceVoiceを生成
	IXAudio2SourceVoice *pSourceVoiceRaw = nullptr;
	result = xAudio2_->CreateSourceVoice(&pSourceVoiceRaw, &soundData.wfex);
	if (FAILED(result)) {
		return;
	}

	std::unique_ptr<IXAudio2SourceVoice, SourceVoiceDeleter> pSourceVoice(pSourceVoiceRaw);

	// バッファ設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer.get();
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;
	buf.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

	result = pSourceVoice->SubmitSourceBuffer(&buf);
	if (FAILED(result)) {
		return;
	}

	pSourceVoice->SetVolume(volume);
	result = pSourceVoice->Start();
	if (FAILED(result)) {
		return;
	}

	// BGMマップで管理
	bgmVoices_[filename] = std::move(pSourceVoice);
}

void AudioManager::StopBGMVoice(const std::string &filename) {
	auto it = bgmVoices_.find(filename);
	if (it != bgmVoices_.end()) {
		if (it->second) {
			it->second->Stop();
		}
		bgmVoices_.erase(it);
	}
}

void AudioManager::PlayBGM(const std::string &filename, bool loop, float volume) {
	// リクエストされたBGM情報を記録（エディタ停止中などでも保持し、PLAY時に再生できるようにする）
	requestedBGMs_[filename] = { loop, volume };

	// 再生が許可されている場合のみ実際にボイスを再生
	if (isBgmPlaybackAllowed_) {
		StartBGMVoice(filename, loop, volume);
	}
}

void AudioManager::StopBGM(const std::string &filename) {
	requestedBGMs_.erase(filename);
	StopBGMVoice(filename);
}

void AudioManager::StopAllBGM() {
	requestedBGMs_.clear();
	for (auto &pair : bgmVoices_) {
		if (pair.second) {
			pair.second->Stop();
		}
	}
	bgmVoices_.clear();
}

void AudioManager::PauseBGM(const std::string &filename) {
	auto it = bgmVoices_.find(filename);
	if (it != bgmVoices_.end() && it->second) {
		it->second->Stop();
	}
}

void AudioManager::ResumeBGM(const std::string &filename) {
	if (isBgmPlaybackAllowed_) {
		auto it = bgmVoices_.find(filename);
		if (it != bgmVoices_.end() && it->second) {
			it->second->Start();
		}
	}
}

void AudioManager::SetBGMVolume(const std::string &filename, float volume) {
	auto itReq = requestedBGMs_.find(filename);
	if (itReq != requestedBGMs_.end()) {
		itReq->second.volume = volume;
	}

	auto it = bgmVoices_.find(filename);
	if (it != bgmVoices_.end() && it->second) {
		it->second->SetVolume(volume);
	}
}

float AudioManager::GetBGMVolume(const std::string &filename) {
	auto it = bgmVoices_.find(filename);
	if (it != bgmVoices_.end() && it->second) {
		float vol = 0.0f;
		it->second->GetVolume(&vol);
		return vol;
	}
	auto itReq = requestedBGMs_.find(filename);
	if (itReq != requestedBGMs_.end()) {
		return itReq->second.volume;
	}
	return 0.0f;
}

bool AudioManager::IsBGMPlaying(const std::string &filename) {
	auto it = bgmVoices_.find(filename);
	if (it != bgmVoices_.end() && it->second) {
		XAUDIO2_VOICE_STATE state{};
		it->second->GetState(&state);
		return state.BuffersQueued > 0;
	}
	return false;
}

void AudioManager::SetBGMPlaybackAllowed(bool allowed) {
	if (isBgmPlaybackAllowed_ == allowed) {
		return;
	}
	isBgmPlaybackAllowed_ = allowed;

	if (isBgmPlaybackAllowed_) {
		// 再生許可状態になった（PLAYが押された等）：保持されていたリクエストを一斉に再生開始
		for (const auto &pair : requestedBGMs_) {
			StartBGMVoice(pair.first, pair.second.loop, pair.second.volume);
		}
	} else {
		// 再生禁止状態になった（STOPが押された等）：再生中のボイスのみをすべて停止・破棄
		for (auto &pair : bgmVoices_) {
			if (pair.second) {
				pair.second->Stop();
			}
		}
		bgmVoices_.clear();
	}
}

bool AudioManager::IsBGMPlaybackAllowed() {
	return isBgmPlaybackAllowed_;
}

void AudioManager::Update() {
	// 再生が終わったSEボイスをリストから削除する
	playingVoices_.remove_if([](const auto &voice) {
		XAUDIO2_VOICE_STATE state{};
		voice->GetState(&state);
		// バッファのキューが空になったら再生終了とみなす
		return state.BuffersQueued == 0;
	});

	// 単発BGMで再生終了したボイスをマップから削除
	for (auto it = bgmVoices_.begin(); it != bgmVoices_.end(); ) {
		if (it->second) {
			XAUDIO2_VOICE_STATE state{};
			it->second->GetState(&state);
			if (state.BuffersQueued == 0) {
				it = bgmVoices_.erase(it);
				continue;
			}
		}
		++it;
	}
}