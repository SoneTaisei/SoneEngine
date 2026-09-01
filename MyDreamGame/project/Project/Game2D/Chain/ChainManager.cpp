#include "ChainManager.h"
#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "GameObject/Object3D.h"
#include "Input/KeyboardInput.h"
#include "Core/Utility/UtilityFunctions.h"
#include <algorithm>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void ChainManager::Initialize(Player2D* player, const Vector3& worldChainBase) {
    player_ = player;
    worldChainBase_ = worldChainBase;
    droppedChains_.clear();
    worldChains_.clear();
    droppedCounter_ = 0;
    loggedSocketState_ = false;

    ChainConfig::Load(params_, ChainConfig::kDefaultFilePath);

    initialChainLength_ = player_ ? player_->GetChainLength() : 3;

    // プレイヤーの鎖（手に追従。ユニット数は chainLength_ と Reconcile で同期）
    ChainParams playerParams = params_;
    playerParams.initialUnits_ = std::clamp(initialChainLength_, params_.minUnits_, params_.maxUnits_);
    playerChain_ = std::make_unique<Chain2D>();
    playerChain_->SetAnchorMode(ChainAnchorMode::kSocket); // Initialize前に設定して初期モードとして記憶させる
    Vector3 start = player_ ? player_->GetPosition() : Vector3{ 2.0f, 5.0f, 0.0f };
    playerChain_->Initialize(start, playerParams, "PlayerChain");
    playerChain_->SetPlayerCollisionSkipCount(playerParams.nodesPerUnit_);

    // ワールド吊り鎖（テスト配置。拾うとユニットをもらえる）
    ChainParams paramsA = params_;
    paramsA.initialUnits_ = 3;
    auto chainA = std::make_unique<Chain2D>();
    chainA->Initialize(worldChainBase_ + Vector3{ 3.0f, 4.0f, 0.0f }, paramsA, "Chain_A");
    worldChains_.push_back(std::move(chainA));

    ChainParams paramsB = params_;
    paramsB.initialUnits_ = 4;
    auto chainB = std::make_unique<Chain2D>();
    chainB->Initialize(worldChainBase_ + Vector3{ 6.0f, 4.0f, 0.0f }, paramsB, "Chain_B");
    worldChains_.push_back(std::move(chainB));
}

void ChainManager::HandleInput() {
    if (!player_ || player_->IsDead()) {
        return;
    }
    KeyboardInput* keyboard = KeyboardInput::GetInstance();

    // 拾う（K）：範囲内に鎖がなければ何もしない（誤って外してしまう誤爆を防ぐ）
    if (keyboard->IsKeyPressed(DIK_K)) {
        TryPickup();
    }

    // 外す（S・下キー）：拾うとは独立したボタン
    // （Jは録画スロットをShiftと共有しており、リプレイ再生時にShift入力が幻の「外す」になるためSを使う）
    if (keyboard->IsKeyPressed(DIK_S) || keyboard->IsKeyPressed(DIK_DOWN)) {
        DetachUnits();
    }
}

bool ChainManager::TryPickup() {
    // 拾う判定は「プレイヤーのAABB（体の箱）から鎖ノードまでの距離」で行う
    // （中心点からの距離だと足元に横たわる鎖が見た目より拾えず、判定がずれて感じるため）
    const AABB2D playerBox = player_->GetAABB();
    int headroom = params_.maxUnits_ - player_->GetChainLength();
    if (headroom <= 0) {
        return false;
    }

    // 落ちている自由鎖 → 吊り鎖 の順に範囲内を探す
    for (size_t i = 0; i < droppedChains_.size(); ++i) {
        if (droppedChains_[i].chain->FindNearestNodeToAABB(playerBox, params_.pickupRadius_)) {
            // 上限を超える分は消滅させる（見えないジャンプペナルティだけが増えるのを防ぐ）
            int gain = (std::min)(droppedChains_[i].unitWorth, headroom);
            player_->AddChainLength(gain);
            droppedChains_.erase(droppedChains_.begin() + i);
            return true; // 個数が増えた分は Reconcile が伸ばす
        }
    }
    for (size_t i = 0; i < worldChains_.size(); ++i) {
        Chain2D* chain = worldChains_[i].get();
        if (chain->FindNearestNodeToAABB(playerBox, params_.pickupRadius_)) {
            // 吊り鎖からまとめてもらう（残りは吊られたまま。アンカー側から縮む）
            int take = (std::min)({ (std::max)(1, params_.unitsPerAction_), chain->GetUnitCount(), headroom });
            if (take > 0) {
                chain->RemoveUnitsAtAnchor(take);
                player_->AddChainLength(take);
                // 使い切った吊り鎖も消さずに残す（アンカー1ノードだけの休眠状態＝描画も物理も判定も無効）
                // ResetToInitial() が初期ユニット数へ復元するので、リプレイ再生や2回目のプレイで世界がずれない
                return true;
            }
        }
    }
    return false;
}

void ChainManager::DetachUnits() {
    if (!playerChain_ || !player_) {
        return;
    }
    // 一度の操作で unitsPerAction_ ユニットをつながったまま外す（minUnits_ は必ず残す）
    int detachable = playerChain_->GetUnitCount() - params_.minUnits_;
    int detach = (std::min)((std::max)(1, params_.unitsPerAction_), detachable);
    if (detach <= 0) {
        // 最後の1本は外せない（SE・揺れ演出は将来対応）
        Log("ChainManager: これ以上鎖を外せない（minUnits）\n");
        return;
    }

    auto removed = playerChain_->RemoveUnitsAtAnchor(detach);
    if (removed.size() < 2) {
        // ノードが足りず自由鎖にできない場合は個数を減らさない（Reconcileがプレイヤー鎖を元に戻す）
        return;
    }
    // 先に個数を減らしてから（同フレームの Reconcile が二重に削らないよう current == target にする）
    player_->AddChainLength(-detach);

    // 切り離したノード列を「つながったままの1本」の自由鎖として生成
    // （pos/prevPos維持 = 切り離し時の速度を引き継ぐ。パラメータは切り離し元のプレイヤー鎖と揃える）
    auto dropped = std::make_unique<Chain2D>();
    dropped->InitializeFromNodes(std::move(removed), playerChain_->GetParams(),
                                 "DroppedChain_" + std::to_string(droppedCounter_++));
    droppedChains_.push_back({ std::move(dropped), detach });
}

void ChainManager::Reconcile() {
    if (!player_ || !playerChain_) {
        return;
    }
    // 保持個数そのものを上下限で正規化する
    // （上限超過を放置すると、見た目の鎖は伸びないのにジャンプペナルティだけが増えてしまう）
    int length = std::clamp(player_->GetChainLength(), params_.minUnits_, params_.maxUnits_);
    if (length != player_->GetChainLength()) {
        player_->SetChainLength(length);
    }
    playerChain_->SetUnitCount(length); // 差分だけアンカー側で伸縮（等しければ何もしない）
}

void ChainManager::Update(float dt, MapChip2D* map) {
    // ソケット同期（プレイヤーモデルの行列更新は UpdateWithMap 内で完了している）
    lastSocketWorld_ = ComputeSocketWorld();

    if (playerChain_) {
        playerChain_->SyncSocket(lastSocketWorld_);
        playerChain_->Update(dt, map, player_);
    }
    for (auto& chain : worldChains_) {
        chain->Update(dt, map, player_);
    }
    for (auto& dropped : droppedChains_) {
        dropped.chain->Update(dt, map, player_);
    }
}

void ChainManager::Draw() {
    if (playerChain_) {
        playerChain_->Draw();
    }
    for (auto& chain : worldChains_) {
        chain->Draw();
    }
    for (auto& dropped : droppedChains_) {
        dropped.chain->Draw();
    }
}

void ChainManager::ResetAll() {
    droppedChains_.clear();

    // 個数を初期値に戻す（リプレイはK入力を録画から再現するため、初期個数の一致が再現性の前提）
    if (player_) {
        player_->SetChainLength(initialChainLength_);
    }
    if (playerChain_) {
        playerChain_->ResetToInitial(); // kSocket・初期ユニット数へ。次のSyncSocketのワープ検出が手元へ引き寄せる
    }
    for (auto& chain : worldChains_) {
        chain->ResetToInitial();
    }
    // 吊り鎖が拾われて消えていた場合は復元しない（マップ再構築を伴わないため）。
    // 完全復元が必要になったら初期配置リストから作り直す形にする
}

void ChainManager::OnRewindEnd() {
    // 巻き戻し中の鎖アクションは再現できないため、落ちている鎖は消して速度をリセットする（10日規模の割り切り）
    droppedChains_.clear();
    if (playerChain_) {
        playerChain_->ResetDynamics();
    }
    for (auto& chain : worldChains_) {
        chain->ResetDynamics();
    }
}

std::vector<Object3D*> ChainManager::GetLinkObjects() const {
    std::vector<Object3D*> result;
    if (playerChain_) {
        auto links = playerChain_->GetLinkObjects();
        result.insert(result.end(), links.begin(), links.end());
    }
    for (auto& chain : worldChains_) {
        auto links = chain->GetLinkObjects();
        result.insert(result.end(), links.begin(), links.end());
    }
    for (auto& dropped : droppedChains_) {
        auto links = dropped.chain->GetLinkObjects();
        result.insert(result.end(), links.begin(), links.end());
    }
    return result;
}

Vector3 ChainManager::ComputeSocketWorld() {
    socketValid_ = false;
    if (player_) {
        if (Object3D* model = player_->GetModelObject()) {
            if (auto pos = model->GetJointWorldPosition(kSocketJointName)) {
                socketValid_ = true;
                if (!loggedSocketState_) {
                    Log(std::string("ChainManager: socket joint '") + kSocketJointName + "' found\n");
                    loggedSocketState_ = true;
                }
                return *pos;
            }
        }
    }

    if (!loggedSocketState_) {
        // ジョイント名のtypo検出用（jointMapの一覧はモデル読み込み時にVS出力へ出ている）
        Log(std::string("ChainManager: socket joint '") + kSocketJointName +
            "' NOT found. Falling back to player position\n");
        loggedSocketState_ = true;
    }

    // フォールバック：プレイヤー中心のやや上
    Vector3 p = player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
    p.y += 0.2f;
    p.z = 0.0f;
    return p;
}

void ChainManager::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("Chain Manager");
    ImGui::Text("K : 拾う / S : 外す（%dユニットずつ）", params_.unitsPerAction_);
    if (player_ && playerChain_) {
        ImGui::Text("ChainLength: %d (units: %d, min %d / max %d)",
                    player_->GetChainLength(), playerChain_->GetUnitCount(),
                    params_.minUnits_, params_.maxUnits_);
    }
    ImGui::Text("Socket: %s (%.2f, %.2f)", socketValid_ ? "joint" : "fallback",
                lastSocketWorld_.x, lastSocketWorld_.y);
    ImGui::Text("World: %d  Dropped: %d",
                static_cast<int>(worldChains_.size()), static_cast<int>(droppedChains_.size()));

    ImGui::DragFloat("Pickup Radius##Mgr", &params_.pickupRadius_, 0.05f, 0.1f, 3.0f);
    ImGui::DragInt("Units Per Action##Mgr", &params_.unitsPerAction_, 1, 1, 8);
    ImGui::DragInt("Max Units##Mgr", &params_.maxUnits_, 1, 1, 16);
    ImGui::DragInt("Min Units##Mgr", &params_.minUnits_, 1, 1, 4);
    // ImGuiのDragIntはキーボード入力で範囲外の値も入るため、min>max等で
    // std::clampが未定義動作にならないよう毎回正規化する
    params_.unitsPerAction_ = (std::max)(1, params_.unitsPerAction_);
    params_.maxUnits_ = (std::max)(1, params_.maxUnits_);
    params_.minUnits_ = std::clamp(params_.minUnits_, 1, params_.maxUnits_);

    // デバッグ用の個数操作（Reconcileが伸縮を反映する）
    if (ImGui::Button("+1 Unit")) {
        if (player_) player_->AddChainLength(1);
    }
    ImGui::SameLine();
    if (ImGui::Button("-1 Unit")) {
        if (player_) player_->AddChainLength(-1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Params##Mgr")) {
        ChainConfig::Save(params_, ChainConfig::kDefaultFilePath);
    }

    // 各鎖の詳細
    if (playerChain_) {
        playerChain_->DrawImGui();
    }
    for (auto& chain : worldChains_) {
        chain->DrawImGui();
    }
    for (auto& dropped : droppedChains_) {
        dropped.chain->DrawImGui();
    }
#endif
}
