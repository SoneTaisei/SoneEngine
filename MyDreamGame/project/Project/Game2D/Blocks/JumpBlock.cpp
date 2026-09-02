#include "JumpBlock.h"
#include "BlockFactory.h"
#include "Game2D/Player/Player2D.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

// BlockFactoryへの自動登録マクロ（プロジェクト起動時に登録されます）
REGISTER_BLOCK_CLASS(JumpBlock);

void JumpBlock::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {
    gameObject_ = std::make_unique<GameObject>("JumpBlock");
    auto* tc = gameObject_->AddComponent<TransformComponent>();
    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();

    prc->Initialize(device, boxPrimitive);
    prc->GetMaterial().color = { 0.40f, 0.70f, 0.90f, 1.00f };
    tc->SetScale({ width, height, 1.0f });
    tc->SetPosition({ worldX, worldY, 0.0f });
    prc->GetMaterial().lightingType = 1;

    SetupCollider();
}

void JumpBlock::Update() {
    // 基底クラスの更新（GameObjectの更新と描画キューへの登録）
    BaseBlock::Update();

    if (!isActive_) return;

    // TODO: ここに毎フレームの更新ロジックを記述します
    timer_ += 1.0f / 60.0f;
}

void JumpBlock::OnCollision(Player2D* player) {
    // TODO: プレイヤーが接触した瞬間の処理（ダメージ、反発、アイテム取得など）
}

void JumpBlock::OnPlayerStand() {
    // TODO: プレイヤーがこのブロックの上に乗っている間の処理（ジャンプ台、加速床、崩れる足場など）
}

void JumpBlock::OnPlayerTouch() {
    // TODO: プレイヤーが横や下から触れた時の処理
}

void JumpBlock::SetProperties(const nlohmann::json& properties) {
    // エディタのインスペクターからプロパティを読み込む処理
    if (properties.contains("customPower") && properties["customPower"].is_number()) {
        customPower_ = properties["customPower"].get<float>();
    }
    if (properties.contains("speed") && properties["speed"].is_number()) {
        speed_ = properties["speed"].get<float>();
    }
}

void JumpBlock::Reset() {
    // TODO: ステージ再開時・リトライ時の状態初期化
    timer_ = 0.0f;
    isActive_ = true;
}

#ifdef USE_IMGUI
void JumpBlock::DrawImGui() {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%s ロジック調整]", "JumpBlock");
    ImGui::DragFloat("カスタムパワー", &customPower_, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("スピード", &speed_, 0.1f, 0.0f, 50.0f);
    ImGui::Checkbox("有効フラグ", &isActive_);
    ImGui::Text("内部タイマー: %.2f 秒", timer_);
    if (ImGui::Button("状態リセット (Reset)")) {
        Reset();
    }
}
#endif
