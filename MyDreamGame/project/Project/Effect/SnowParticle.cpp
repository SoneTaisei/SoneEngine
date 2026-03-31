#include "SnowParticle.h"

void SnowParticle::Update() {
    const float kDeltaTime = 1.0f / 60.0f; // 固定フレームレート前提

    // ■■■ 1. 新規発生処理 (追加) ■■■
    // エミッタのタイマーを進める
    emitter_.frequencyTime += kDeltaTime;
    // 設定された頻度(frequency)を超えたら発生させる
    if(emitter_.frequencyTime >= emitter_.frequency) {
        // 設定された個数分、リストに追加
        // ※Emitterの設定はInitializeなどでされている前提ですが、
        //   ここでお好みで調整してもOKです
        emitter_.frequencyTime -= emitter_.frequency; // タイマーリセット


        // 発生場所をランダムに変えたい場合はここで emitter_.transform.translate をいじる
        // 例: カメラの上空から降らせるなど

        Emit(emitter_); // 親クラスのEmitを呼ぶ
    }

    // ■■■ 2. 既存パーティクルの更新 (元の処理) ■■■
    for(auto it = particles_.begin(); it != particles_.end(); ) {
        // ... (省略：元の移動処理や寿命チェック) ...

        // 寿命チェック
        if(it->lifeTime <= it->currentTime) {
            it = particles_.erase(it);
            continue;
        }

        // 移動処理
        it->velocity.y = -2.0f;
        it->velocity.x = std::sin(it->currentTime * 5.0f) * 0.5f;

        it->transform.translate.x += it->velocity.x * kDeltaTime;
        it->transform.translate.y += it->velocity.y * kDeltaTime;
        it->transform.translate.z += it->velocity.z * kDeltaTime;

        it->currentTime += kDeltaTime;
        it->color = { 1.0f,1.0f,1.0f,0.5f };
        ++it;
    }
}
