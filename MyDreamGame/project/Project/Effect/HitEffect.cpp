#include "HitEffect.h"
#include <numbers>

#ifdef USE_IMGUI
#include "imgui.h"
#include "Editor/EditorManager.h"
#endif

void HitEffect::Update() {
    // まず親の更新を呼ぶ（移動や寿命処理）
    ParticleManager::Update();

#ifdef USE_IMGUI
    // エディターがPlay中でなければ何もしない
    if (!EditorManager::IsPlaying()) {
        return;
    }
#endif

    // 1秒ごとに自動発生させるタイマー処理
    autoEmitTimer_ += 1.0f / 60.0f;
    if (autoEmitTimer_ >= 1.0f) {
        autoEmitTimer_ = 0.0f;
        EmitHit(testEmitPosition_);
        EmitSlash(testEmitPosition_);
    }

    // 1秒(lifeTime)で徐々に透明になって消える処理
    for (auto& p : particles_) {
        float alpha = 1.0f - (p.currentTime / p.lifeTime);
        if (alpha < 0.0f) alpha = 0.0f;
        p.color.w = alpha;
    }
}

void HitEffect::Draw(const Matrix4x4& viewProjection) {
    // 強制的に加算ブレンドモードにする
    blendMode_ = kBlendModeAdd;
    ParticleManager::Draw(viewProjection);
}

void HitEffect::EmitHit(const Vector3& position) {
    std::uniform_real_distribution<float> distRotate(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> distScale(scaleYMin_, scaleYMax_);

    // 設定された数だけ発生させる
    for (int i = 0; i < emitCountHit_; ++i) {
        ParticleData particle = MakeNewParticle(position);
        particle.transform.translate = position; // 位置を完全に重ねる
        particle.transform.scale = { scaleX_, distScale(randomEngine_), 1.0f }; // 横に潰す、縦はランダム
        particle.transform.rotate = { 0.0f, 0.0f, distRotate(randomEngine_) }; // Z軸回転
        particle.velocity = { 0.0f, 0.0f, 0.0f }; // 動かない
        particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        particle.lifeTime = lifeTime_; // パラメータの寿命
        particle.currentTime = 0.0f;

        particles_.push_back(particle);
    }
}

void HitEffect::EmitSlash(const Vector3& position) {
    std::uniform_real_distribution<float> distScale(scaleYMin_, scaleYMax_);
    std::uniform_real_distribution<float> distRotate(-0.2f, 0.2f); // 少し傾きにランダム性を持たせる

    for (int i = 0; i < emitCountSlash_; ++i) {
        ParticleData particle = MakeNewParticle(position);
        particle.transform.translate = position; // 位置を完全に重ねる
        particle.transform.scale = { scaleX_, distScale(randomEngine_), 1.0f };
        particle.transform.rotate = { 0.0f, 0.0f, distRotate(randomEngine_) };
        particle.velocity = { 0.0f, 0.0f, 0.0f };
        particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        particle.lifeTime = lifeTime_ * 0.6f; // 少し短めにするなど
        particle.currentTime = 0.0f;

        particles_.push_back(particle);
    }
}

void HitEffect::DrawImGui() {
    ParticleManager::DrawImGui(); // 親のImGuiも呼ぶ（ビルボード設定などがあるため）

#ifdef USE_IMGUI
    if (ImGui::TreeNode("HitEffect Parameters")) {
        ImGui::DragFloat("Scale X", &scaleX_, 0.01f, 0.01f, 1.0f);
        ImGui::DragFloat("Scale Y Min", &scaleYMin_, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("Scale Y Max", &scaleYMax_, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("Life Time", &lifeTime_, 0.01f, 0.1f, 5.0f);
        ImGui::DragInt("Emit Count Hit", &emitCountHit_, 1, 1, 32);
        ImGui::DragInt("Emit Count Slash", &emitCountSlash_, 1, 1, 32);

        ImGui::Separator();
        ImGui::DragFloat3("Test Position", &testEmitPosition_.x, 0.1f);
        if (ImGui::Button("Test Emit Hit")) {
            EmitHit(testEmitPosition_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Test Emit Slash")) {
            EmitSlash(testEmitPosition_);
        }

        ImGui::TreePop();
    }
#endif
}
