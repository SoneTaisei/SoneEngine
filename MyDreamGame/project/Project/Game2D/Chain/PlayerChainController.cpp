#include "PlayerChainController.h"
#include "Game2D/Chain/Chain2D.h"
#include "Game2D/Player/Player2D.h"
#include "GameObject/Object3D.h"
#include "Input/KeyboardInput.h"
#include "Core/Utility/UtilityFunctions.h"
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void PlayerChainController::Initialize(Player2D* player) {
    player_ = player;
    heldChain_ = nullptr;
    socketValid_ = false;
    loggedSocketState_ = false;
}

void PlayerChainController::Update(std::vector<std::unique_ptr<Chain2D>>& chains) {
    if (!player_) {
        return;
    }

    // 1. ソケット（手のジョイント）座標を取得
    //    プレイヤーモデルのObject3D::Update()はUpdateWithMap内で済んでいるため、この時点で最新
    lastSocketWorld_ = ComputeSocketWorld();

    // 2. 拾う/落とすトグル入力（死亡中は操作不可。Kはリプレイ録画対象の'C'スロット）
    if (!player_->IsDead() && KeyboardInput::GetInstance()->IsKeyPressed(DIK_K)) {
        if (heldChain_) {
            // 落とす：その場でフリー状態になり、地面へ落ちる
            heldChain_->SetAnchorMode(ChainAnchorMode::kFree);
            heldChain_ = nullptr;
        } else {
            // 拾う：いずれかのノードが範囲内にある最寄りの鎖を掴む
            Chain2D* best = nullptr;
            float bestDistSq = pickupRange_ * pickupRange_;
            const Vector3& playerPos = player_->GetPosition();
            for (auto& chain : chains) {
                for (int i = 0; i < chain->GetNodeCount(); ++i) {
                    Vector3 d = chain->GetNodePosition(i) - playerPos;
                    float distSq = d.x * d.x + d.y * d.y;
                    if (distSq < bestDistSq) {
                        bestDistSq = distSq;
                        best = chain.get();
                    }
                }
            }
            if (best) {
                best->SetAnchorMode(ChainAnchorMode::kSocket);
                best->SyncSocket(lastSocketWorld_);
                best->ResetDynamics(); // 掴んだ瞬間の暴れ防止
                heldChain_ = best;
            }
        }
    }

    // 3. 保持中はソケットへ毎フレーム同期（この後の chain->Update() が反映する）
    if (heldChain_) {
        heldChain_->SyncSocket(lastSocketWorld_);
    }
}

Vector3 PlayerChainController::ComputeSocketWorld() {
    socketValid_ = false;
    if (Object3D* model = player_->GetModelObject()) {
        if (auto pos = model->GetJointWorldPosition(kSocketJointName)) {
            socketValid_ = true;
            if (!loggedSocketState_) {
                Log(std::string("PlayerChainController: socket joint '") + kSocketJointName + "' found\n");
                loggedSocketState_ = true;
            }
            return *pos;
        }
    }

    if (!loggedSocketState_) {
        // ジョイント名のtypo検出用（jointMapの一覧はモデル読み込み時にVS出力へ出ている）
        Log(std::string("PlayerChainController: socket joint '") + kSocketJointName +
            "' NOT found. Falling back to player position\n");
        loggedSocketState_ = true;
    }

    // フォールバック：プレイヤー中心のやや上
    Vector3 p = player_->GetPosition();
    p.y += 0.2f;
    p.z = 0.0f;
    return p;
}

void PlayerChainController::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("Player Chain");
    ImGui::Text("K : 拾う / 落とす");
    ImGui::Text("Holding: %s", heldChain_ ? "YES" : "no");
    ImGui::Text("Socket: %s (%.2f, %.2f)", socketValid_ ? "joint" : "fallback",
                lastSocketWorld_.x, lastSocketWorld_.y);
    ImGui::DragFloat("Pickup Range", &pickupRange_, 0.05f, 0.3f, 5.0f);
#endif
}
