#include "FragileBlock.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/Security/AlertSystem.h"
#include "Editor/Replay/ReplayManager.h"
#include <algorithm>
#include <cmath>
#include <random>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

int FragileBlock::s_currentChainWeight = -1;
int FragileBlock::s_previewChainWeight = -1;
bool FragileBlock::s_highlightAll = false;
bool FragileBlock::s_debugNoBreak = false;

namespace {
    constexpr int kMaxPips = 8;              // 表示する点の上限（鎖の上限本数と同じ）
    constexpr int kPipsPerRow = 4;
    constexpr float kPipPitch = 0.17f;
    constexpr float kPipSize = 0.11f;
    constexpr float kFrontZ = -0.56f;        // 前面（カメラ側は -z）
    constexpr float kShakeAmplitude = 0.1f;  // 震えの最大幅（時間が経つほど激しくなる）
    const Vector4 kBreakColor = {1.0f, 0.2f, 0.2f, 1.0f}; // 落ちる直前の色

    const Vector4 kPipColor = {0.10f, 0.10f, 0.10f, 1.0f};
    const Vector4 kPipDangerColor = {1.0f, 0.85f, 0.25f, 1.0f};
    const Vector4 kCrackColor = {0.16f, 0.12f, 0.09f, 1.0f};

    Vector4 Lerp(const Vector4& a, const Vector4& b, float t) {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
    }

    // ひびの配置（ブロック中心からの割合と傾き）
    struct CrackDef { float ox, oy, rot, len; };
    const CrackDef kCracks[2] = {
        {-0.14f, 0.22f, 0.65f, 0.55f},
        {0.16f, 0.14f, -0.35f, 0.45f},
    };
}

FragileBlock::FragileBlock(MapChip2D* map, int chipX, int chipY)
    : BaseBlock(map, chipX, chipY) {}

std::unique_ptr<GameObject> FragileBlock::MakePart(const Vector3& scale, const Vector4& color) {
    auto part = std::make_unique<GameObject>();
    part->Initialize();
    part->SetName("FragileDecor");
    auto* tc = part->AddComponent<TransformComponent>();
    tc->SetPosition({startX_, startY_, kFrontZ});
    tc->SetScale(scale);
    auto* r = part->AddComponent<PrimitiveRendererComponent>();
    r->Initialize(device_, boxPrimitive_);
    r->GetMaterial().color = color;
    r->GetMaterial().lightingType = 1;
    return part;
}

void FragileBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    startX_ = worldX;
    startY_ = worldY;
    device_ = device;
    boxPrimitive_ = boxPrimitive;

    gameObject_ = std::make_unique<GameObject>();
    gameObject_->Initialize();
    gameObject_->SetName("FragileBlock");

    auto* transform = gameObject_->AddComponent<TransformComponent>();
    transform->SetPosition({worldX, worldY, 0.0f});
    transform->SetScale({width, height, 1.0f});

    auto* renderer = gameObject_->AddComponent<PrimitiveRendererComponent>();
    renderer->Initialize(device, boxPrimitive);
    renderer->GetMaterial().color = baseColor_; // パレットの色は生成後に上書きされる（最初の Update で取り込む）
    renderer->GetMaterial().lightingType = 1;

    SetupCollider();

    // ひび（見分け用）
    cracks_.clear();
    for (const auto& c : kCracks) {
        cracks_.push_back(MakePart({c.len * width, 0.045f, 0.06f}, kCrackColor));
    }
    SyncPips();
    LayoutDecor({startX_, startY_, 0.0f}, 0.0f, false);
}

void FragileBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("breakWeight") && properties["breakWeight"].is_number()) {
        breakWeight_ = properties["breakWeight"];
    }
    if (properties.contains("breakDuration") && properties["breakDuration"].is_number()) {
        breakDuration_ = properties["breakDuration"];
    }
    breakWeight_ = std::clamp(breakWeight_, 1, kMaxPips + 1);
    breakDuration_ = (std::max)(breakDuration_, 0.05f);
    SyncPips();
    LayoutDecor({startX_, startY_, 0.0f}, 0.0f, false);
}

void FragileBlock::SetBreakWeight(int weight) {
    breakWeight_ = std::clamp(weight, 1, kMaxPips + 1);
    SyncPips();
    if (!isBreaking_) {
        LayoutDecor({startX_, startY_, 0.0f}, 0.0f, false);
    }
}

void FragileBlock::SyncPips() {
    // インスペクタのプレビュー用ブロックは Initialize されないので、その時は何も作らない
    if (!device_ || !boxPrimitive_) return;
    int wanted = std::clamp(breakWeight_ - 1, 0, kMaxPips);
    while (static_cast<int>(pips_.size()) > wanted) {
        pips_.pop_back();
    }
    while (static_cast<int>(pips_.size()) < wanted) {
        pips_.push_back(MakePart({kPipSize, kPipSize, 0.06f}, kPipColor));
    }
}

void FragileBlock::LayoutDecor(const Vector3& center, float rotZ, bool danger) {
    float cs = std::cos(rotZ);
    float sn = std::sin(rotZ);
    float sx = 1.0f;
    float sy = 1.0f;
    if (gameObject_) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            sx = tc->GetScale().x;
            sy = tc->GetScale().y;
        }
    }
    auto place = [&](GameObject* obj, float ox, float oy, float extraRot) {
        float rx = ox * cs - oy * sn;
        float ry = ox * sn + oy * cs;
        if (auto* tc = obj->GetComponent<TransformComponent>()) {
            tc->SetPosition({center.x + rx, center.y + ry, kFrontZ});
            tc->SetRotation({0.0f, 0.0f, rotZ + extraRot});
        }
        obj->Update();
    };

    // ひび（上半分）
    for (size_t i = 0; i < cracks_.size() && i < 2; ++i) {
        place(cracks_[i].get(), kCracks[i].ox * sx, kCracks[i].oy * sy, kCracks[i].rot);
    }

    // 点（下半分に並べる。4個で1列）
    int n = static_cast<int>(pips_.size());
    if (n == 0) return;
    int rows = (n + kPipsPerRow - 1) / kPipsPerRow;
    float baseY = -0.20f * sy;
    Vector4 pipColor = danger ? kPipDangerColor : kPipColor;
    for (int i = 0; i < n; ++i) {
        int r = i / kPipsPerRow;
        int c = i % kPipsPerRow;
        int inRow = (std::min)(kPipsPerRow, n - r * kPipsPerRow);
        float ox = (static_cast<float>(c) - (inRow - 1) * 0.5f) * kPipPitch;
        float oy = baseY + ((rows - 1) * 0.5f - static_cast<float>(r)) * kPipPitch;
        if (auto* pr = pips_[i]->GetComponent<PrimitiveRendererComponent>()) {
            pr->GetMaterial().color = pipColor;
        }
        place(pips_[i].get(), ox, oy, 0.0f);
    }
}

void FragileBlock::Update() {
    BaseBlock::Update();
    if (isDestroyed_ || !gameObject_) return;

    auto* tc = gameObject_->GetComponent<TransformComponent>();
    auto* renderer = gameObject_->GetComponent<PrimitiveRendererComponent>();
    if (!baseColorCaptured_ && renderer) {
        baseColor_ = renderer->GetMaterial().color;
        baseColorCaptured_ = true;
    }

    // エディタ用の点滅（全部強調／選択中）。ゲーム中は使わない
    float blink = 0.0f;
    Vector4 blinkColor = {1.0f, 0.9f, 0.3f, 1.0f};
#ifdef USE_IMGUI
    if (selected_ || hovered_ || s_highlightAll) {
        double t = ImGui::GetTime();
        float speed = selected_ ? 10.0f : (hovered_ ? 8.0f : 5.0f);
        blink = 0.5f * (1.0f + static_cast<float>(std::sin(t * speed)));
        if (selected_) {
            blinkColor = {1.0f, 1.0f, 1.0f, 1.0f};
        } else if (hovered_) {
            blinkColor = {0.5f, 0.9f, 1.0f, 1.0f};
        }
    }
    hovered_ = false; // 毎フレーム GameScene 側が立て直す
#endif

    bool danger = false;
    float rotZ = 0.0f;
    Vector3 pos = {startX_, startY_, 0.0f};

    if (isBreaking_) {
        // リプレイ再生・シーク時も録画時と同じだけ時間が進むよう、共有クロックの差分を使う
        float dt = ReplayManager::GetInstance()->GetPlayDeltaTime();
        breakTimer_ += dt;

        if (breakTimer_ >= breakDuration_) {
            if (s_debugNoBreak) {
                // デバッグ用：消えずに元に戻る（何度でも試せる）
                Reset();
                return;
            }
            // 落ちて消える（近くに警備員がいれば騒音）
            isDestroyed_ = true;
            if (auto* alert = AlertSystem::Current()) {
                alert->AddNoise({startX_, startY_, 0.0f}, map_, "騒音");
            }
            return;
        }
        // 震える演出：ランダムに揺れ（時間が経つほど激しく）、だんだん赤くなる。この間はまだ乗れる
        float progress = breakTimer_ / breakDuration_;
        static std::mt19937 randEngine(std::random_device{}());
        std::uniform_real_distribution<float> dist(-kShakeAmplitude * progress, kShakeAmplitude * progress);
        pos = {startX_ + dist(randEngine), startY_ + dist(randEngine), 0.0f};
        if (tc) {
            tc->SetPosition(pos);
        }
        if (renderer) {
            renderer->GetMaterial().color = Lerp(baseColor_, kBreakColor, progress);
        }
        danger = true;
    } else {
        // 今の本数（エディタの試し本数があればそちら）で乗ると崩れるなら赤く染めて予告する（乗る前に分かるように）
        int weight = (s_previewChainWeight >= 0) ? s_previewChainWeight : s_currentChainWeight;
        danger = (weight >= 0 && weight >= breakWeight_);
        if (renderer) {
            Vector4 color = danger ? Lerp(baseColor_, {0.95f, 0.25f, 0.2f, 1.0f}, 0.55f) : baseColor_;
            if (blink > 0.0f) {
                color = Lerp(color, blinkColor, blink * 0.7f);
            }
            renderer->GetMaterial().color = color;
        }
        if (tc) {
            pos = tc->GetPosition();
        }
    }

    LayoutDecor(pos, rotZ, danger);
}

void FragileBlock::Draw() {
    BaseBlock::Draw();
    if (isDestroyed_) return;
    for (auto& crack : cracks_) {
        crack->Draw();
    }
    for (auto& pip : pips_) {
        pip->Draw();
    }
}

void FragileBlock::OnPlayerStand(Player2D* player) {
    if (isBreaking_ || !player) return;
    // 鎖の本数が上限以上なら震え始める（breakDuration_ 後に落ちる）。少なければ普通に通れる
    if (player->GetChainLength() >= breakWeight_) {
        isBreaking_ = true;
        breakTimer_ = 0.0f;
    }
}

void FragileBlock::Reset() {
    isDestroyed_ = false;
    isBreaking_ = false;
    breakTimer_ = 0.0f;

    if (gameObject_) {
        auto* tc = gameObject_->GetComponent<TransformComponent>();
        if (tc) {
            tc->SetPosition({startX_, startY_, 0.0f});
            tc->SetRotation({0.0f, 0.0f, 0.0f});
        }
        auto* renderer = gameObject_->GetComponent<PrimitiveRendererComponent>();
        if (renderer) renderer->GetMaterial().color = baseColor_;
    }
    LayoutDecor({startX_, startY_, 0.0f}, 0.0f, false);
}

#ifdef USE_IMGUI
void FragileBlock::DrawImGui() {
    // パレット側の値（breakWeight / breakDuration）は MapEditorInspector が自動生成する。ここでは意味だけ示す
    ImGui::TextDisabled("崩れる床：breakWeight = この本数以上の鎖で乗ると崩れる（通れるのは breakWeight-1 本まで）");
    ImGui::TextDisabled("1枚ずつ上限を変えるには GameScene の「Fragile Floors」を使う");
}
#endif

void FragileBlock::CaptureReplayState(std::vector<float>& outCustom) const {
    outCustom.clear();
    outCustom.push_back(isBreaking_ ? 1.0f : 0.0f);
    outCustom.push_back(breakTimer_);
}

void FragileBlock::RestoreReplayState(const std::vector<float>& custom) {
    if (custom.size() < 2) return;
    isBreaking_ = (custom[0] != 0.0f);
    breakTimer_ = custom[1];
}
