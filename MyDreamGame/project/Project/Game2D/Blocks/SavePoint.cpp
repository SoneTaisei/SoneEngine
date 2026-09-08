#include "SavePoint.h"
#include "BlockFactory.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "Core/TimeManager.h"
#include "Scenes/GameScene.h"
#include <filesystem>
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#include "Editor/EditorManager.h"
#endif

// BlockFactoryへの自動登録マクロ（プロジェクト起動時に登録されます）
REGISTER_BLOCK_CLASS(SavePoint);

// 静的セーブデータの初期化
std::unordered_map<std::string, SavePoint::CheckpointData> SavePoint::s_ActiveCheckpoints;

std::string SavePoint::NormalizeStageKey(const std::string& stagePath) {
    if (stagePath.empty()) return "";
    try {
        std::filesystem::path p(stagePath);
        std::string stem = p.stem().string();
        return stem.empty() ? stagePath : stem;
    } catch (...) {
        return stagePath;
    }
}

void SavePoint::SetActiveSavePoint(const std::string& stagePath, const Vector3& pos, int chipX, int chipY, int chainLength) {
    std::string key = NormalizeStageKey(stagePath);
    if (key.empty()) return;
    s_ActiveCheckpoints[key] = { true, pos, chipX, chipY, chainLength };
}

int SavePoint::GetActiveChainLength(const std::string& stagePath) {
    std::string key = NormalizeStageKey(stagePath);
    auto it = s_ActiveCheckpoints.find(key);
    if (it == s_ActiveCheckpoints.end() || !it->second.isValid) {
        return -1;
    }
    return it->second.chainLength;
}

bool SavePoint::HasActiveSavePoint(const std::string& stagePath) {
    std::string key = NormalizeStageKey(stagePath);
    if (key.empty()) return false;
    auto it = s_ActiveCheckpoints.find(key);
    return (it != s_ActiveCheckpoints.end() && it->second.isValid);
}

Vector3 SavePoint::GetActiveSavePoint(const std::string& stagePath) {
    std::string key = NormalizeStageKey(stagePath);
    if (key.empty()) return { 0.0f, 0.0f, 0.0f };
    auto it = s_ActiveCheckpoints.find(key);
    if (it != s_ActiveCheckpoints.end() && it->second.isValid) {
        return it->second.spawnPosition;
    }
    return { 0.0f, 0.0f, 0.0f };
}

int SavePoint::GetActiveChipX(const std::string& stagePath) {
    std::string key = NormalizeStageKey(stagePath);
    auto it = s_ActiveCheckpoints.find(key);
    if (it != s_ActiveCheckpoints.end() && it->second.isValid) {
        return it->second.chipX;
    }
    return -1;
}

int SavePoint::GetActiveChipY(const std::string& stagePath) {
    std::string key = NormalizeStageKey(stagePath);
    auto it = s_ActiveCheckpoints.find(key);
    if (it != s_ActiveCheckpoints.end() && it->second.isValid) {
        return it->second.chipY;
    }
    return -1;
}

void SavePoint::Clear(const std::string& stagePath) {
    if (stagePath.empty()) {
        s_ActiveCheckpoints.clear();
    } else {
        std::string key = NormalizeStageKey(stagePath);
        s_ActiveCheckpoints.erase(key);
    }
}

void SavePoint::ClearAll() {
    s_ActiveCheckpoints.clear();
}

std::string SavePoint::GetCurrentStageKey() const {
    // エディタの停止→再生では一時ファイル temp_play_map を読むので、それは鍵にしない。
    // 記録する時は「temp_play_map」、探す時は本当のステージ名、と食い違って
    // セーブポイントを通ったのに無い扱いになってしまうため
    if (map_) {
        const std::string& loaded = map_->GetCurrentFilePath();
        if (!loaded.empty() && loaded.find("temp_play_map") == std::string::npos) {
            return NormalizeStageKey(loaded);
        }
    }
    return NormalizeStageKey(GameScene::s_TargetMapFilePath);
}

bool SavePoint::CheckIfCurrentActive() const {
    std::string key = GetCurrentStageKey();
    if (key.empty()) return false;
    auto it = s_ActiveCheckpoints.find(key);
    if (it != s_ActiveCheckpoints.end() && it->second.isValid) {
        return (it->second.chipX == chipX_ && it->second.chipY == chipY_);
    }
    return false;
}

bool SavePoint::IsActive() const {
    return CheckIfCurrentActive();
}

void SavePoint::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    centerX_ = worldX;
    centerY_ = worldY;
    baseWidth_ = width;
    baseHeight_ = height;

    gameObject_ = std::make_unique<GameObject>("SavePoint");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    // 初期カラー：淡いシアン
    prc->GetMaterial().color = { 0.25f, 0.70f, 1.0f, 0.85f };
    prc->GetMaterial().lightingType = 1;
    prc->GetMaterial().shininess = 32.0f;

    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });

    SetupCollider();

    if (CheckIfCurrentActive()) {
        isActivated_ = true;
    }
}

void SavePoint::Update() {
    BaseBlock::Update();
    if (!gameObject_) return;

#ifdef USE_IMGUI
    // エディターでプレイが押されていない時はアニメーションしない
    if (!EditorManager::IsPlaying()) {
        if (auto* tc = gameObject_->GetComponent<TransformComponent>()) {
            tc->SetScale({ baseWidth_, baseHeight_, 1.0f });
            tc->SetPosition({ centerX_, centerY_, 0.0f });
            tc->SetRotation({ 0.0f, 0.0f, 0.0f });
        }
        if (auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>()) {
            bool isCurrent = CheckIfCurrentActive();
            prc->GetMaterial().color = isCurrent
                ? Vector4{ 0.25f, 1.0f, 0.50f, 1.0f }
                : Vector4{ 0.25f, 0.70f, 1.0f, 0.85f };
        }
        return;
    }
#endif

    float dt = TimeManager::GetInstance().GetDeltaTime();
    animTime_ += dt;

    if (pulseTimer_ > 0.0f) {
        pulseTimer_ = (std::max)(0.0f, pulseTimer_ - dt);
    }

    auto* tc = gameObject_->GetComponent<TransformComponent>();
    auto* prc = gameObject_->GetComponent<PrimitiveRendererComponent>();
    if (!tc || !prc) return;

    bool isCurrent = CheckIfCurrentActive();
    if (isCurrent) {
        isActivated_ = true;
    }

    // 浮遊アニメーション（上下にやさしく揺れる）
    float bob = std::sin(animTime_ * 3.0f) * 0.06f;

    // 自転アニメーション（起動中は少し速く回る）
    float rotSpeed = isCurrent ? 2.2f : 1.0f;
    float rotY = animTime_ * rotSpeed;

    // パルス拡大効果（触れた直後にポワンと大きくなる）
    float pulseFactor = (pulseTimer_ > 0.0f) ? (pulseTimer_ / 0.5f) : 0.0f;
    float scaleMul = 1.0f + pulseFactor * 0.35f;

    tc->SetScale({ baseWidth_ * scaleMul, baseHeight_ * scaleMul, 1.0f });
    tc->SetPosition({ centerX_, centerY_ + bob, 0.0f });
    tc->SetRotation({ 0.0f, rotY, 0.0f });

    // カラーの決定
    Vector4 baseColor;
    if (isCurrent) {
        // 現在アクティブなセーブポイント：輝くエメラルドグリーン
        float glow = 0.85f + 0.15f * std::sin(animTime_ * 5.0f);
        baseColor = { 0.25f * glow, 1.0f * glow, 0.50f * glow, 1.0f };
    } else if (isActivated_) {
        // 過去に触れたが他の中間ポイントがアクティブ：落ち着いた緑
        baseColor = { 0.35f, 0.75f, 0.55f, 0.75f };
    } else {
        // 未起動：静かに脈動するシアンブルー
        float pulse = 0.85f + 0.15f * std::sin(animTime_ * 2.0f);
        baseColor = { 0.20f * pulse, 0.65f * pulse, 1.0f * pulse, 0.85f };
    }

    // 触れた瞬間の白フラッシュブレンド
    if (pulseFactor > 0.0f) {
        baseColor.x = baseColor.x + (1.0f - baseColor.x) * pulseFactor;
        baseColor.y = baseColor.y + (1.0f - baseColor.y) * pulseFactor;
        baseColor.z = baseColor.z + (1.0f - baseColor.z) * pulseFactor;
    }

    prc->GetMaterial().color = baseColor;
}

void SavePoint::Activate(Player2D* player) {
    if (!player) return;

    bool isAlreadyCurrent = CheckIfCurrentActive();
    if (isAlreadyCurrent) return;

    isActivated_ = true;
    pulseTimer_ = 0.5f;

    // スポーン位置はセーブポイントの中心（必要に応じてオフセット付与）
    Vector3 spawnPos = { centerX_, centerY_ + customSpawnOffsetY_, 0.0f };
    std::string key = GetCurrentStageKey();
    // 通った時の鎖の本数も一緒に覚える（ここから再開した時に同じ本数で始められる）
    SetActiveSavePoint(key, spawnPos, chipX_, chipY_, player->GetChainLength());

    // プレイヤーの復帰開始位置を更新
    player->SetStartPosition(spawnPos);
}

void SavePoint::OnCollision(Player2D* player) {
    Activate(player);
}

void SavePoint::OnPlayerStand() {
}

void SavePoint::OnPlayerStand(Player2D* player) {
    Activate(player);
}

void SavePoint::OnPlayerTouch() {
}

void SavePoint::OnPlayerTouch(Player2D* player) {
    Activate(player);
}

void SavePoint::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("spawnOffsetY") && properties["spawnOffsetY"].is_number()) {
        customSpawnOffsetY_ = properties["spawnOffsetY"].get<float>();
    }
}

void SavePoint::Reset() {
    pulseTimer_ = 0.0f;
    // ステージリスタート時も、このポイントがアクティブであれば状態を維持
    if (CheckIfCurrentActive()) {
        isActivated_ = true;
    }
}

#ifdef USE_IMGUI
void SavePoint::DrawImGui() {
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "[中間ポイント (SavePoint)]");
    bool isCurrent = CheckIfCurrentActive();
    ImGui::Text("現在のアクティブ状態: %s", isCurrent ? "★ 有効 (Active)" : "待機中 (Standby)");
    ImGui::Text("配置チップ座標: (%d, %d)", chipX_, chipY_);
    ImGui::Text("中心座標: (%.2f, %.2f)", centerX_, centerY_);
    ImGui::DragFloat("リスポーンYオフセット", &customSpawnOffsetY_, 0.05f, -2.0f, 2.0f);

    if (ImGui::Button("この地点を中間ポイントに設定")) {
        Vector3 spawnPos = { centerX_, centerY_ + customSpawnOffsetY_, 0.0f };
        SetActiveSavePoint(GetCurrentStageKey(), spawnPos, chipX_, chipY_);
        pulseTimer_ = 0.5f;
    }

    ImGui::SameLine();
    if (ImGui::Button("セーブポイント解除")) {
        Clear(GetCurrentStageKey());
        isActivated_ = false;
        pulseTimer_ = 0.0f;
    }
}
#endif
