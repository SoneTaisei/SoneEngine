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

void ChainManager::Initialize(Player2D* player) {
    player_ = player;
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

    // お宝（鎖の末端ノードに描画する重り。モデルは ChainParams のパスから）
    treasure_ = std::make_unique<Treasure2D>();
    treasure_->Initialize(params_.treasureModelDir_, params_.treasureModelFile_, params_.treasureScale_);

    // スピンジャンプ
    spin_ = std::make_unique<ChainSpinAction>();
    spin_->Initialize(params_);

    ApplyTreasureParams();
    SyncTreasureTransform();

    // 吊り鎖はマップ配置で追加する（AddWorldChain）。テスト用の仮配置は廃止
}

void ChainManager::AddWorldChain(const Vector3& anchorPos, int units, const std::string& name) {
    ChainParams worldParams = params_;
    worldParams.initialUnits_ = (std::max)(1, units);
    // 吊り鎖は伸びない（拾われて縮むだけ）ので、リンクの事前確保は必要分だけにする
    // （Chain2D::Initialize は maxUnits_ 分を確保する。1本あたり約16MBの定数バッファ節約）
    worldParams.maxUnits_ = worldParams.initialUnits_;
    auto chain = std::make_unique<Chain2D>();
    chain->Initialize(anchorPos, worldParams, name);
    worldChains_.push_back(std::move(chain));
}

void ChainManager::HandleInput() {
    if (!player_ || player_->IsDead() || player_->IsGoal()) {
        return;
    }
    KeyboardInput* keyboard = KeyboardInput::GetInstance();

    // スピン（W / ↑キー）：押し続けて構え、A/D で重りを振り、離して発射。縁の検出は ChainSpinAction 側
    // （'W'スロットは W と ↑ のみで他用途と競合しない。A/D は 'L'/'R' スロットで録画される）
    if (spin_) {
        bool spinHeld = keyboard->IsKeyDown(DIK_W) || keyboard->IsKeyDown(DIK_UP);
        spin_->SetKeyHeld(spinHeld);

        float swing = 0.0f;
        if (keyboard->IsKeyDown(DIK_D) || keyboard->IsKeyDown(DIK_RIGHT)) swing += 1.0f;
        if (keyboard->IsKeyDown(DIK_A) || keyboard->IsKeyDown(DIK_LEFT)) swing -= 1.0f;
        spin_->SetSwingInput(swing);
    }

    // 拾う（K）：範囲内に鎖がなければ何もしない（誤って外してしまう誤爆を防ぐ）
    if (keyboard->IsKeyPressed(DIK_K)) {
        if (spin_) spin_->Cancel(player_, playerChain_.get()); // 構え中の着脱は中断してから
        TryPickup();
    }

    // 外す（J / S / 下キー）：拾うとは独立したボタン
    // 注意: Jの録画スロットはShiftと共有のため、将来ダッシュ等でShiftを使うと
    // リプレイ再生時に幻の「外す」になり得る。その場合はJを外してS(下)だけにする
    if (keyboard->IsKeyPressed(DIK_J) ||
        keyboard->IsKeyPressed(DIK_S) || keyboard->IsKeyPressed(DIK_DOWN)) {
        if (spin_) spin_->Cancel(player_, playerChain_.get());
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
            Log("ChainManager: Picked up dropped chain +" + std::to_string(gain) + " unit(s), chainLength=" +
                std::to_string(player_->GetChainLength()) + "\n");
            return true; // 個数が増えた分は Reconcile が繰り出す
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
    // 繰り出し中は外せない（繰り出し待ちと実ノードが混在すると落とす鎖の長さが個数と合わなくなるため）
    if (playerChain_->IsPayingOut()) {
        Log("ChainManager: 繰り出し中は外せない\n");
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

    Log("ChainManager: Detached " + std::to_string(detach) + " unit(s), chainLength=" +
        std::to_string(player_->GetChainLength()) + "\n");
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
    playerChain_->SetUnitCount(length); // 増える分は繰り出しキューへ、減る分はアンカー側から削除（等しければ何もしない）
}

void ChainManager::Update(float dt, MapChip2D* map) {
    // ソケット同期（プレイヤーモデルの行列更新は UpdateWithMap 内で完了している）
    lastSocketWorld_ = ComputeSocketWorld();

    // スピン：末端の拘束先を物理更新の前に決める
    if (spin_) {
        spin_->Update(dt, map, player_, playerChain_.get(), lastSocketWorld_);
    }

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

    // お宝の見た目：構え中は振りに合わせて自転させ、発射の勢いが十分な間は明るくして「今離せば強く飛ぶ」合図にする
    if (treasure_ && spin_) {
        treasure_->SetHighlight(spin_->IsLaunchReady());
        if (spin_->IsInStance()) {
            treasure_->AddSelfRotation(spin_->GetOmega() * dt);
        }
    }
    SyncTreasureTransform();
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
    if (treasure_) {
        treasure_->Draw();
    }
}

void ChainManager::ResetAll() {
    if (spin_) {
        spin_->Cancel(player_, playerChain_.get());
        spin_->ResetInputState(); // 0フレーム目の縁検出を録画/再生で揃える
    }
    droppedChains_.clear();

    // 個数を初期値に戻す（リプレイはK入力を録画から再現するため、初期個数の一致が再現性の前提）
    if (player_) {
        player_->SetChainLength(initialChainLength_);
    }
    if (playerChain_) {
        playerChain_->ResetToInitial(); // kSocket・初期ユニット数へ（繰り出し状態もクリア）。次のSyncSocketのワープ検出が手元へ引き寄せる
    }
    for (auto& chain : worldChains_) {
        chain->ResetToInitial(); // 使い切って休眠していた吊り鎖もここで復活する
    }
    if (treasure_) {
        treasure_->SetHighlight(false);
    }
    SyncTreasureTransform();
}

void ChainManager::OnRewindBegin() {
    if (spin_) {
        spin_->Cancel(player_, playerChain_.get());
        spin_->ResetInputState();
    }
}

void ChainManager::OnRewindEnd() {
    // 巻き戻し中の鎖アクションは再現できないため、落ちている鎖は消して速度をリセットする（10日規模の割り切り）
    if (spin_) {
        spin_->Cancel(player_, playerChain_.get());
        spin_->ResetInputState();
    }
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
    if (treasure_ && treasure_->GetObject()) {
        result.push_back(treasure_->GetObject());
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

void ChainManager::ApplyTreasureParams() {
    if (playerChain_) {
        EndWeight weight;
        weight.enabled = true;
        weight.mass = params_.treasureMass_;
        weight.radius = params_.treasureRadius_;
        weight.friction = params_.treasureFriction_;
        weight.ignorePlayer = params_.treasureIgnorePlayer_;
        playerChain_->SetEndWeight(weight);
        playerChain_->SetPayoutSpeed(params_.payoutSpeed_);
        // 持っている鎖はプレイヤーと当たらない（回した重りや鎖が体に引っかからない）。落ちている鎖・吊り鎖は当たる
        playerChain_->SetPlayerCollisionEnabled(params_.heldChainPlayerCollision_);
    }
    if (treasure_) {
        treasure_->SetVisualScale(params_.treasureScale_);
    }
    if (spin_) {
        spin_->SetParams(params_); // 宝石の質量はスピンの振りにくさにも使う
    }
}

void ChainManager::SyncTreasureTransform() {
    if (!treasure_ || !playerChain_) {
        return;
    }
    int n = playerChain_->GetNodeCount();
    if (n < 2) {
        return;
    }
    treasure_->UpdateTransform(playerChain_->GetEndPosition(), playerChain_->GetNodePosition(n - 2));
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
    ImGui::Text("K : 拾う / J・S : 外す（%dユニットずつ） / W(↑)押し続け+A/D : 重りを振る → W離して発射", params_.unitsPerAction_);
    if (player_ && playerChain_) {
        ImGui::Text("ChainLength: %d (units: %d, min %d / max %d)%s",
                    player_->GetChainLength(), playerChain_->GetUnitCount(),
                    params_.minUnits_, params_.maxUnits_,
                    playerChain_->IsPayingOut() ? "  [繰り出し中]" : "");
    }
    ImGui::Text("Socket: %s (%.2f, %.2f)", socketValid_ ? "joint" : "fallback",
                lastSocketWorld_.x, lastSocketWorld_.y);
    ImGui::Text("World: %d  Dropped: %d",
                static_cast<int>(worldChains_.size()), static_cast<int>(droppedChains_.size()));

    ImGui::DragFloat("Pickup Radius##Mgr", &params_.pickupRadius_, 0.05f, 0.1f, 3.0f);
    ImGui::DragInt("Units Per Action##Mgr", &params_.unitsPerAction_, 1, 1, 8);
    ImGui::DragInt("Max Units##Mgr", &params_.maxUnits_, 1, 1, 16);
    ImGui::DragInt("Min Units##Mgr", &params_.minUnits_, 1, 1, 4);
    if (ImGui::DragFloat("Payout Speed##Mgr", &params_.payoutSpeed_, 0.1f, 0.5f, 30.0f)) {
        params_.payoutSpeed_ = (std::max)(0.1f, params_.payoutSpeed_);
        if (playerChain_) playerChain_->SetPayoutSpeed(params_.payoutSpeed_);
    }
    // ImGuiのDragIntはキーボード入力で範囲外の値も入るため、min>max等で
    // std::clampが未定義動作にならないよう毎回正規化する
    params_.unitsPerAction_ = (std::max)(1, params_.unitsPerAction_);
    params_.maxUnits_ = (std::max)(1, params_.maxUnits_);
    params_.minUnits_ = std::clamp(params_.minUnits_, 1, params_.maxUnits_);
    if (spin_) spin_->SetParams(params_); // minUnits_ 等はスピン側でも使うので同期する

    // お宝（重り）
    ImGui::SeparatorText("Treasure (End Weight)");
    bool treasureChanged = false;
    treasureChanged |= ImGui::DragFloat("Mass##Treasure", &params_.treasureMass_, 0.1f, 0.1f, 50.0f);
    treasureChanged |= ImGui::DragFloat("Radius##Treasure", &params_.treasureRadius_, 0.01f, 0.05f, 1.0f);
    treasureChanged |= ImGui::DragFloat("Friction##Treasure", &params_.treasureFriction_, 0.01f, 0.0f, 1.0f);
    treasureChanged |= ImGui::Checkbox("Ignore Player##Treasure", &params_.treasureIgnorePlayer_);
    treasureChanged |= ImGui::Checkbox("Held Chain Collides Player (持ち鎖とプレイヤーの判定)##Treasure", &params_.heldChainPlayerCollision_);
    treasureChanged |= ImGui::DragFloat("Visual Scale##Treasure", &params_.treasureScale_, 0.01f, 0.01f, 2.0f);
    ImGui::Text("Model: %s / %s", params_.treasureModelDir_.c_str(), params_.treasureModelFile_.c_str());
    if (treasureChanged) {
        params_.treasureMass_ = (std::max)(0.1f, params_.treasureMass_);
        params_.treasureRadius_ = (std::max)(0.05f, params_.treasureRadius_);
        params_.treasureFriction_ = std::clamp(params_.treasureFriction_, 0.0f, 1.0f);
        params_.treasureScale_ = (std::max)(0.01f, params_.treasureScale_);
        ApplyTreasureParams();
    }
    ImGui::Text("Treasure pos: (%.2f, %.2f)", GetTreasurePosition().x, GetTreasurePosition().y);

    // スピンジャンプ
    ImGui::SeparatorText("Spin Jump");
    if (spin_) {
        spin_->DrawImGui();
    }
    bool spinChanged = false;
    ImGui::TextDisabled("張った鎖を 振る力 / (宝石の質量 + 鎖の質量) で漕ぐ。離すと鎖ごと |角速度| x 半径 で飛び、Delay 秒後に重りの進行方向へ 重りの速さ x Transfer で引かれる（上限 = ジャンプ初速 x Ratio）");
    spinChanged |= ImGui::DragFloat("Spin Radius Max##Spin", &params_.spinRadiusMax_, 0.05f, 0.3f, 10.0f);
    spinChanged |= ImGui::DragFloat("Spin Radius Ratio##Spin", &params_.spinRadiusRatio_, 0.01f, 0.3f, 1.0f);
    spinChanged |= ImGui::DragFloat("Swing Strength##Spin", &params_.swingStrength_, 0.5f, 0.0f, 200.0f);
    spinChanged |= ImGui::DragFloat("Swing Damping##Spin", &params_.swingDamping_, 0.01f, 0.0f, 5.0f);
    spinChanged |= ImGui::DragFloat("Chain Mass Per Unit##Spin", &params_.chainMassPerUnit_, 0.05f, 0.0f, 10.0f);
    spinChanged |= ImGui::DragFloat("Weight Throw Scale##Spin", &params_.weightThrowScale_, 0.05f, 0.0f, 3.0f);
    spinChanged |= ImGui::DragFloat("Pull Delay##Spin", &params_.pullDelay_, 0.01f, 0.0f, 1.0f);
    spinChanged |= ImGui::DragFloat("Pull Transfer##Spin", &params_.pullTransfer_, 0.05f, 0.0f, 3.0f);
    spinChanged |= ImGui::DragFloat("Launch Max Jump Ratio##Spin", &params_.launchMaxJumpRatio_, 0.05f, 0.1f, 1.7f);
    spinChanged |= ImGui::DragFloat("Launch Min Upward##Spin", &params_.launchMinUpward_, 0.01f, 0.0f, 1.0f);
    spinChanged |= ImGui::DragFloat("Stance Move Factor##Spin", &params_.spinMoveFactor_, 0.05f, 0.0f, 1.0f);
    spinChanged |= ImGui::DragFloat("Spin Cooldown##Spin", &params_.spinCooldown_, 0.05f, 0.0f, 3.0f);
    if (spinChanged) {
        // ImGuiのキーボード入力で不正値が入っても NaN や壁すり抜けにならないよう、ChainConfig::Load と同じ正規化を行う
        params_.spinRadiusMax_ = (std::max)(0.3f, params_.spinRadiusMax_);
        params_.spinRadiusRatio_ = std::clamp(params_.spinRadiusRatio_, 0.3f, 1.0f);
        params_.swingStrength_ = (std::max)(0.0f, params_.swingStrength_);
        params_.swingDamping_ = (std::max)(0.0f, params_.swingDamping_);
        params_.chainMassPerUnit_ = (std::max)(0.0f, params_.chainMassPerUnit_);
        params_.weightThrowScale_ = (std::max)(0.0f, params_.weightThrowScale_);
        params_.pullDelay_ = std::clamp(params_.pullDelay_, 0.0f, 1.0f);
        params_.pullTransfer_ = (std::max)(0.0f, params_.pullTransfer_);
        params_.launchMaxJumpRatio_ = std::clamp(params_.launchMaxJumpRatio_, 0.1f, 1.7f); // 1.7×17.5≒30 u/s が壁すり抜けの上限
        params_.launchMinUpward_ = std::clamp(params_.launchMinUpward_, 0.0f, 1.0f);
        params_.spinMoveFactor_ = std::clamp(params_.spinMoveFactor_, 0.0f, 1.0f);
        params_.spinCooldown_ = (std::max)(0.0f, params_.spinCooldown_);
        if (spin_) spin_->SetParams(params_);
    }

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
