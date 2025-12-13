#include "windowParticle.h"

void windowParticle::Update() {
    // -------------------------------------------------
    // 1. 新しいパーティクルの発生処理 (Emitter)
    // -------------------------------------------------
    // 1/60秒単位で処理が進むと仮定 (可変フレームレートなら引数でdeltaTimeをもらう必要があります)
    const float kDeltaTime = 1.0f / 60.0f;

    emitter_.frequencyTime += kDeltaTime;

    // 発生頻度(frequency)を超えたらパーティクルを生成
    if(emitter_.frequencyTime >= emitter_.frequency) {
        emitter_.frequencyTime -= emitter_.frequency;

        // 色をランダムにするための分布 (0.0 ～ 1.0)
        std::uniform_real_distribution<float> distColor(0.0f, 1.0f);

        // 指定された数だけパーティクルを生成してリストに追加
        for(uint32_t i = 0; i < emitter_.count; ++i) {
            // 親クラスの関数を使って基本データを生成 (位置や速度など)
            ParticleData p = MakeNewParticle(emitter_.transform.translate);

            // ★ここで色をランダムに上書きする (RGBをランダム、Alphaは1.0)
            p.color = { distColor(randomEngine_), distColor(randomEngine_), distColor(randomEngine_), 1.0f };

            // リストに追加
            particles_.push_back(p);
        }
    }

    // -------------------------------------------------
    // 2. 既存のパーティクルの更新処理
    // -------------------------------------------------
    // リスト内の全パーティクルを走査
    for(auto it = particles_.begin(); it != particles_.end(); ) {

        // --- 寿命の処理 ---
        it->currentTime += kDeltaTime;
        if(it->currentTime >= it->lifeTime) {
            // 寿命が尽きたら削除して次の要素へ
            it = particles_.erase(it);
            continue;
        }

        // --- 移動処理 (ここがWindowParticle独自の挙動になります) ---

        if(IsCollision(accelerationField_.area, it->transform.translate)) {
            // エリア内なら、風の加速度を加える
            it->velocity.x += accelerationField_.acceleration.x * kDeltaTime;
            it->velocity.y += accelerationField_.acceleration.y * kDeltaTime;
            it->velocity.z += accelerationField_.acceleration.z * kDeltaTime;
        }

        // 例: 速度を加算して移動させる
        it->transform.translate.x += it->velocity.x * kDeltaTime;
        it->transform.translate.y += it->velocity.y * kDeltaTime;
        it->transform.translate.z += it->velocity.z * kDeltaTime;

        // 例: 徐々に透明にする処理
        // lifeTimeに対するcurrentTimeの割合でアルファ値を減らす
        float alpha = 1.0f - (it->currentTime / it->lifeTime);
        it->color.w = std::clamp(alpha, 0.0f, 1.0f);

        // 次の要素へ
        ++it;
    }
}