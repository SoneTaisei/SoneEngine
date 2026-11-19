#include "SnowParticle.h"
#include "Core/TimeManager.h"

void SnowParticle::Update() {
    float dt = Time::GetDeltaTime();
    if (dt <= 0.0f) {
        return;
    }
    GetEmitter().frequencyTime += dt;
    // 設定された頻度(frequency)を超えたら発生させる
    if(GetEmitter().frequencyTime >= GetEmitter().frequency) {
        // 設定された個数分、リストに追加
        // ※Emitterの設定はInitializeなどでされている前提ですが、
        //   ここでお好みで調整してもOKです
        GetEmitter().frequencyTime -= GetEmitter().frequency; // タイマーリセット


        // 発生場所をランダムに変えたい場合はここで GetEmitter().transform.translate をいじる
        // 例: カメラの上空から降らせるなど

        Emit(GetEmitter()); // 親クラスのEmitを呼ぶ
    }

    // ■■■ 2. 既存パーティクルの更新 (元の処理) ■■■
    for(auto it = GetParticles().begin(); it != GetParticles().end(); ) {
        // ... (省略：元の移動処理や寿命チェック) ...

        // 寿命チェック
        if(it->lifeTime <= it->currentTime) {
            it = GetParticles().erase(it);
            continue;
        }

        // 移動処理
        it->velocity.y = -2.0f;
        it->velocity.x = std::sin(it->currentTime * 5.0f) * 0.5f;

        it->transform.translate.x += it->velocity.x * dt;
        it->transform.translate.y += it->velocity.y * dt;
        it->transform.translate.z += it->velocity.z * dt;

        it->currentTime += dt;
        float alpha = 1.0f - (it->currentTime / it->lifeTime);
        it->color.w = alpha;
        ++it;
    }
}
