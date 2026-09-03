#include "MovingBlock.h"
#include "Game2D/Player/Player2D.h"
#include "Editor/Replay/ReplayManager.h"
#include <cmath>
#ifdef USE_IMGUI
#include <imgui.h>
#include "Editor/EditorManager.h"
#endif

MovingBlock::MovingBlock(MapChip2D* map, int chipX, int chipY)
    : BaseBlock(map, chipX, chipY) {}

void MovingBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    startX_ = worldX;
    startY_ = worldY;
    prevPosition_ = {worldX, worldY, 0.0f};
    deltaPosition_ = {0.0f, 0.0f, 0.0f};
    hasPrevPosition_ = false;

    gameObject_ = std::make_unique<GameObject>();
    gameObject_->Initialize();
    gameObject_->SetName("MovingBlock");

    auto* transform = gameObject_->AddComponent<TransformComponent>();
    transform->SetPosition({worldX, worldY, 0.0f});
    transform->SetScale({width, height, 1.0f});

    auto* renderer = gameObject_->AddComponent<PrimitiveRendererComponent>();
    renderer->Initialize(device, boxPrimitive);
    renderer->GetMaterial().color = {0.8f, 0.5f, 0.1f, 1.0f}; // オレンジっぽい色
    renderer->GetMaterial().lightingType = 1;

    SetupCollider();
}

void MovingBlock::SetProperties(const nlohmann::json& properties) {
    if (properties.contains("moveAxis") && properties["moveAxis"].is_string()) {
        moveAxis_ = properties["moveAxis"];
    }
    if (properties.contains("moveRange") && properties["moveRange"].is_number()) {
        moveRange_ = properties["moveRange"];
    }
    if (properties.contains("moveSpeed") && properties["moveSpeed"].is_number()) {
        moveSpeed_ = properties["moveSpeed"];
    }
}

Vector3 MovingBlock::CalcPositionAt(float time) const {
    // 配置された初期座標を元にタイミング（位相）をずらす
    float phase = startX_ * 0.5f + startY_ * 0.5f;
    // 指定した moveSpeed_ が「最大速度」になるように角速度を計算 (v = r * ω より ω = v / r)
    float omega = (moveRange_ > 0.0f) ? (moveSpeed_ / moveRange_) : 0.0f;
    float offset = std::sin(time * omega + phase) * moveRange_;

    Vector3 newPos = {startX_, startY_, 0.0f};
    if (moveAxis_ == "X" || moveAxis_ == "x") {
        newPos.x += offset;
    } else if (moveAxis_ == "Y" || moveAxis_ == "y") {
        newPos.y += offset;
    }
    return newPos;
}

void MovingBlock::Update() {
    BaseBlock::Update();
    if (!gameObject_) return;

#ifdef USE_IMGUI
    // エディタモード中は動かない
    if (!EditorManager::IsPlaying()) return;
#endif

    // 自前で deltaTime を積算せず、ReplayManager が持つ共有のゲーム内時刻を使う。
    // 再生中はこの値が記録されたフレームの時刻になるため、
    // シークやループをしても録画時とまったく同じ位置に来る。
    float newTime = ReplayManager::GetInstance()->GetPlayTime();
    float elapsed = newTime - timer_;
    timer_ = newTime;

    auto* tc = gameObject_->GetComponent<TransformComponent>();
    if (!tc) return;

    Vector3 newPos = CalcPositionAt(timer_);

    if (!hasPrevPosition_) {
        // 初回はまだ移動していない扱いにして、速度が跳ね上がらないようにする
        prevPosition_ = newPos;
        hasPrevPosition_ = true;
    }

    deltaPosition_ = {newPos.x - prevPosition_.x, newPos.y - prevPosition_.y, 0.0f};
    if (elapsed > 0.0f) {
        currentVelocity_ = {deltaPosition_.x / elapsed, deltaPosition_.y / elapsed, 0.0f};
    } else {
        // 一時停止・シーク直後は速度を持たせない（プレイヤーが乗っていても押し出さない）
        currentVelocity_ = {0.0f, 0.0f, 0.0f};
    }
    prevPosition_ = newPos;

    tc->SetPosition(newPos);
}

void MovingBlock::CaptureReplayState(std::vector<float>& outCustom) const {
    outCustom.clear();
    outCustom.push_back(timer_);
}

void MovingBlock::RestoreReplayState(const std::vector<float>& custom) {
    if (custom.empty()) return;
    timer_ = custom[0];
    prevPosition_ = CalcPositionAt(timer_);
    deltaPosition_ = {0.0f, 0.0f, 0.0f};
    currentVelocity_ = {0.0f, 0.0f, 0.0f};
    hasPrevPosition_ = true;
}

void MovingBlock::OnPlayerStand(Player2D* player) {
    // 物理エンジン側で velocity を加算するため、ここでの直接座標操作は行わない
    (void)player;
}

#ifdef USE_IMGUI
void MovingBlock::DrawImGui() {
    // 設定は MapEditorInspector 側で自動生成されるため、ここでは何もしません
}
#endif
