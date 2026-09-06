#include "ChainManager.h"
#include "Game2D/Security/AlertSystem.h"
#include "Game2D/Blocks/BaseBlock.h"
#include "Game2D/Blocks/GuardBlock.h"
#include <cmath>

#include "Game2D/Player/Player2D.h"
#include "Game2D/MapChip2D.h"
#include "GameObject/Object3D.h"
#include "Input/KeyboardInput.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Effect/GPUParticle/GPUParticleSystem.h"
#include <algorithm>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

ChainManager::ChainManager() = default;
ChainManager::~ChainManager() = default;

void ChainManager::Initialize(Player2D* player) {
    player_ = player;
    droppedChains_.clear();
    worldChains_.clear();
    droppedCounter_ = 0;
    loggedSocketState_ = false;

    // 破断エフェクトの読み込み
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
    if (device) {
        breakEffect_ = std::make_unique<GPUParticleSystem>();
        breakEffect_->Initialize(device);
        breakEffect_->LoadFromFile("resources/json/shared/Particle/FX_ChainBreak.json");
    }

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
    if (!player_ || player_->IsDead() || player_->IsGoal() || transitionHidden_) {
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

    // 拾う（K）：縛った警備員の近くなら鎖を取り戻す。それ以外は範囲内の鎖を拾う（無ければ何もしない）
    if (keyboard->IsKeyPressed(DIK_K)) {
        if (spin_) spin_->Cancel(player_, playerChain_.get()); // 構え中の着脱は中断してから
        if (!TryUnbindGuard()) {
            TryPickup();
        }
    }


    // 置く（J）：光っている（縛れる）警備員が近ければ縛る、それ以外は外して落とす。取る（K）と隣の右手キーで対にする
    // 注意: Jの録画スロットはShiftと共有のため、将来ダッシュ等でShiftを使うと
    // リプレイ再生時に幻の「外す」になり得る。その場合はJを外してS(下)だけにする
    if (keyboard->IsKeyPressed(DIK_J)) {
        if (spin_) spin_->Cancel(player_, playerChain_.get());
        if (!TryBindGuard()) {
            DetachUnits();
        }
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

namespace {
    bool OverlapAABB(const AABB2D& a, const AABB2D& b) {
        return a.right > b.left && a.left < b.right && a.top > b.bottom && a.bottom < b.top;
    }
    bool CircleOverlapsAABB(const Vector3& c, float r, const AABB2D& box) {
        float cx = std::clamp(c.x, box.left, box.right);
        float cy = std::clamp(c.y, box.bottom, box.top);
        float dx = c.x - cx;
        float dy = c.y - cy;
        return dx * dx + dy * dy < r * r;
    }
    AABB2D Expand(const AABB2D& box, float margin) {
        return {box.left - margin, box.top + margin, box.right + margin, box.bottom - margin};
    }
    GuardBlock* FindOverlappingGuard(MapChip2D* map, const AABB2D& box, bool wantBound) {
        if (!map) return nullptr;
        for (const auto& blockPtr : map->GetUpdateBlocks()) {
            auto* guard = dynamic_cast<GuardBlock*>(blockPtr.get());
            if (!guard || guard->IsDestroyed()) continue;
            if (wantBound != guard->CanUnbind()) continue;
            if (OverlapAABB(box, guard->GetAABB())) return guard;
        }
        return nullptr;
    }
}

bool ChainManager::TryBindGuard() {
    if (!player_ || !playerChain_) {
        return false;
    }
    // 判定は拾うと同じ広さ（体の箱 + pickupRadius_）
    GuardBlock* guard = FindOverlappingGuard(lastMap_, Expand(player_->GetAABB(), params_.pickupRadius_), false);
    if (!guard || !guard->CanBind(player_->GetPosition())) {
        return false;
    }
    // 縛る = 鎖を1ユニット預ける。最後の1本は預けられない（空振り。落としもしない）
    if (player_->GetChainLength() <= params_.minUnits_) {
        Log("ChainManager: bind failed (last chain unit)\n");
        return true;
    }
    if (playerChain_->IsPayingOut()) {
        return true; // 繰り出し中は個数が確定しないので空振り
    }
    guard->Bind(1);
    player_->AddChainLength(-1); // Reconcile が手元側から1ユニット縮める
    Log("ChainManager: guard bound, chainLength=" + std::to_string(player_->GetChainLength()) + "\n");
    return true;
}

bool ChainManager::TryUnbindGuard() {
    if (!player_ || !playerChain_) {
        return false;
    }
    GuardBlock* guard = FindOverlappingGuard(lastMap_, Expand(player_->GetAABB(), params_.pickupRadius_), true);
    if (!guard) {
        return false;
    }
    int units = guard->Unbind();
    int headroom = params_.maxUnits_ - player_->GetChainLength();
    int gain = std::clamp(units, 0, (std::max)(0, headroom));
    if (gain > 0) {
        player_->AddChainLength(gain); // 増えた分は Reconcile が手元から繰り出す
    }
    Log("ChainManager: guard unbound +" + std::to_string(gain) + " unit(s)\n");
    return true;
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

    // 外したエフェクト再生
    if (breakEffect_) {
        Vector3 detachPos = removed.empty() ? lastSocketWorld_ : removed[0].pos;
        breakEffect_->PlayAt(detachPos);
    }

    // 切り離したノード列を「つながったままの1本」の自由鎖として生成
    // （pos/prevPos維持 = 切り離し時の速度を引き継ぐ。パラメータは切り離し元のプレイヤー鎖と揃える）
    auto dropped = std::make_unique<Chain2D>();
    dropped->InitializeFromNodes(std::move(removed), playerChain_->GetParams(),
                                 "DroppedChain_" + std::to_string(droppedCounter_++));
    // その場に落とす：手の移動速度を引き継がない（引き継ぐと歩きながら外した時にぶっ飛ぶ）
    dropped->ResetDynamics();
    // 落とした鎖はプレイヤーに蹴られて動かない（拾う判定には影響しない）
    dropped->SetPlayerCollisionEnabled(false);
    droppedChains_.push_back({ std::move(dropped), detach });

    Log("ChainManager: Detached " + std::to_string(detach) + " unit(s), chainLength=" +
        std::to_string(player_->GetChainLength()) + "\n");
}

void ChainManager::Reconcile() {
    if (!player_ || !playerChain_) {
        return;
    }
    if (tornChain_) {
        return; // ちぎれて死亡中は個数合わせで伸ばさない（復活時に作り直す）
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
    lastMap_ = map;
    // ソケット同期（プレイヤーモデルの行列更新は UpdateWithMap 内で完了している）
    lastSocketWorld_ = ComputeSocketWorld();

    // 遷移中はプレイヤー鎖・お宝・スピンを止める（遷移側の複製が代わりに描かれる。ソケット計算だけは着地目標のため続ける）
    if (transitionHidden_) {
        for (auto& chain : worldChains_) {
            chain->Update(dt, map, player_);
        }
        for (auto& dropped : droppedChains_) {
            dropped.chain->Update(dt, map, player_);
        }
        NotifyBlockContacts(map);
        return;
    }

    // スピン：回せる場所（木の板の上）にいるかを先に判定し、末端の拘束先を物理更新の前に決める
    UpdateSpinSpots(map);
    if (spin_) {
        spin_->Update(dt, map, player_, playerChain_.get(), lastSocketWorld_);
    }

    if (playerChain_) {
        playerChain_->SyncSocket(lastSocketWorld_);
        playerChain_->Update(dt, map, player_);
    }
    if (tornChain_) {
        tornChain_->Update(dt, map, nullptr); // ちぎれた鎖はその場で物理に任せる
    }
    for (auto& chain : worldChains_) {
        chain->Update(dt, map, player_);
    }
    for (auto& dropped : droppedChains_) {
        dropped.chain->Update(dt, map, player_);
    }

    // 鎖が乗っているブロックへ通知（スイッチは鎖でも押せる）
    NotifyBlockContacts(map);

    // 騒音：宝石が速いまま急に止まった（着地・壁に当たった）ら、近くの警備員が反応する
    {
        auto checkNoise = [&](Chain2D* chain, float& prevSpeed) {
            if (!chain) { prevSpeed = 0.0f; return; }
            Vector3 v = chain->GetEndVelocity();
            float speed = std::sqrt(v.x * v.x + v.y * v.y);
            if (auto* alert = AlertSystem::Current()) {
                float threshold = alert->GetParams().noiseSpeed_;
                if (prevSpeed >= threshold && speed < threshold * 0.35f) {
                    alert->AddNoise(chain->GetEndPosition(), map, "騒音");
                }
            }
            prevSpeed = speed;
        };
        checkNoise(playerChain_.get(), prevGemSpeed_);
        checkNoise(tornChain_.get(), prevTornGemSpeed_);
    }

    // テザー（鎖が張ったらプレイヤーが宝石に引かれる。重さの手応え）
    UpdateTether();

    // ちぎれ判定（伸び切った状態が続いたらミス）と、復活時の後始末
    UpdateTear(dt, map);

    // 縛れる／取り戻せる警備員の合図（押す前に分かるように明るくする）
    if (player_ && map && !player_->IsDead()) {
        AABB2D reach = Expand(player_->GetAABB(), params_.pickupRadius_);
        if (GuardBlock* g = FindOverlappingGuard(map, reach, true)) {
            g->SetPrompt(true);
        } else if (GuardBlock* g2 = FindOverlappingGuard(map, reach, false)) {
            if (g2->CanBind(player_->GetPosition()) && player_->GetChainLength() > params_.minUnits_) {
                g2->SetPrompt(true);
            }
        }
    }

    // お宝の見た目：構え中は振りに合わせて自転させ、発射の勢いが十分な間は明るくして「今離せば強く飛ぶ」合図にする
    if (treasure_ && spin_) {
        treasure_->SetHighlight(spin_->IsLaunchReady());
        if (spin_->IsInStance()) {
            treasure_->AddSelfRotation(spin_->GetOmega() * dt);
        }
    }
    SyncTreasureTransform();

    if (breakEffect_ && breakEffect_->IsPlaying()) {
        breakEffect_->Update(dt);
    }
}

void ChainManager::Draw() {
    if (playerChain_ && !transitionHidden_) {
        playerChain_->Draw();
    }
    if (tornChain_) {
        tornChain_->Draw();
    }
    for (auto& chain : worldChains_) {
        chain->Draw();
    }
    for (auto& dropped : droppedChains_) {
        dropped.chain->Draw();
    }
    if (treasure_ && !transitionHidden_) {
        treasure_->Draw();
    }
}

void ChainManager::DrawParticle(ID3D12GraphicsCommandList* commandList, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix, ParticleCommon* particleCommon, ModelManager* modelManager) {
    if (breakEffect_ && breakEffect_->IsPlaying()) {
        breakEffect_->Draw(commandList, viewProjection, cameraMatrix, particleCommon, modelManager);
    }
}

void ChainManager::SetTransitionHidden(bool hidden) {
    if (transitionHidden_ == hidden) {
        return;
    }
    transitionHidden_ = hidden;
    ClearTorn();
    if (spin_) {
        spin_->Cancel(player_, playerChain_.get());
        spin_->ResetInputState();
    }
    if (!hidden && playerChain_) {
        // 手元に垂れた初期姿勢から再開（次の SyncSocket のワープ検出で手元へ引き寄せられる）
        playerChain_->ResetToInitial();
        if (treasure_) {
            treasure_->SetHighlight(false);
        }
        SyncTreasureTransform();
    }
}

void ChainManager::ResetAll() {
    if (spin_) {
        spin_->Cancel(player_, playerChain_.get());
        spin_->ResetInputState(); // 0フレーム目の縁検出を録画/再生で揃える
    }
    ClearTorn();
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
    if (breakEffect_) {
        breakEffect_->Restart();
        breakEffect_->Pause();
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
    if (breakEffect_) {
        breakEffect_->Restart();
        breakEffect_->Pause();
    }
    ClearTorn();
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

void ChainManager::NotifyBlockContacts(MapChip2D* map) {
    if (!map) {
        return;
    }
    // プレイヤーと同じくチップ単位で判定する（節の円が重なるチップのブロックに通知）。動くブロックは対象外
    // 警備員（動くブロック）は別枠：宝石が当たれば「殴る」、落ちている鎖が足元に重なれば「転ばせる」
    auto notify = [&](Chain2D* chain) {
        if (!chain) {
            return;
        }
        const bool isFree = (chain->GetAnchorMode() == ChainAnchorMode::kFree); // 落ちている鎖・ちぎれた鎖
        const auto& nodes = chain->GetNodes();
        const int last = static_cast<int>(nodes.size()) - 1;
        for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
            // 固定されたアンカー（手・吊り点）は鎖ではないので除外。落ちている鎖は先頭も節として扱う
            if (i == 0 && !isFree) {
                continue;
            }
            const VerletNode& node = nodes[i];
            float r = node.radius;
            Vector3 vel = chain->GetNodeVelocity(i);
            float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
            bool isWeight = (i == last) && chain->GetEndWeight().enabled;
            int x0 = map->WorldToChipX(node.pos.x - r);
            int x1 = map->WorldToChipX(node.pos.x + r);
            int y0 = map->WorldToChipY(node.pos.y - r);
            int y1 = map->WorldToChipY(node.pos.y + r);
            for (int cy = y0; cy <= y1; ++cy) {
                for (int cx = x0; cx <= x1; ++cx) {
                    BaseBlock* block = map->GetBlock(cx, cy);
                    if (!block || block->IsDestroyed() || block->IsMoving()) {
                        continue;
                    }
                    if (block->OnChainTouch(node.pos, r, vel, isWeight) && isWeight) {
                        chain->ScaleNodeVelocity(i, 0.4f);
                    }
                }
            }
            for (const auto& blockPtr : map->GetUpdateBlocks()) {
                auto* guard = dynamic_cast<GuardBlock*>(blockPtr.get());
                if (!guard || guard->IsDestroyed()) {
                    continue;
                }
                if (isWeight && !isFree) {
                    // 殴る：手に持っている鎖の宝石が体に当たる（振る・落とす・放つ、全部同じ判定）
                    if (CircleOverlapsAABB(node.pos, r, guard->GetAABB()) && guard->HitByTreasure(vel)) {
                        chain->ScaleNodeVelocity(i, 0.4f); // 跳ね返して連打を防ぐ
                    }
                } else if (isFree) {
                    // 転ばせる：落ちている鎖の節が移動中の足元に重なる
                    if (CircleOverlapsAABB(node.pos, r, guard->GetFootAABB())) {
                        guard->TripByChain(speed);
                    }
                }
            }
        }
    };

    if (!transitionHidden_) {
        notify(playerChain_.get()); // 手に持っている鎖・投げた直後の鎖・末端の宝石
    }
    notify(tornChain_.get()); // ちぎれて落ちた鎖
    for (auto& chain : worldChains_) {
        notify(chain.get());
    }
    for (auto& dropped : droppedChains_) {
        notify(dropped.chain.get());
    }
}

void ChainManager::UpdateSpinSpots(MapChip2D* map) {
    bool allowed = false;
    if (map && player_ && !player_->IsDead()) {
        // 足の直下のチップ（木の板はここにある）か、体が重なるチップに「回せる」ブロックがあれば可
        // （判定はプレイヤーのブロック接触と同じチップ単位。足の直下は 0.1 だけ下を見る）
        AABB2D box = player_->GetAABB();
        int x0 = map->WorldToChipX(box.left);
        int x1 = map->WorldToChipX(box.right);
        int y0 = map->WorldToChipY(box.bottom - 0.1f);
        int y1 = map->WorldToChipY(box.top);
        for (int cy = y0; cy <= y1; ++cy) {
            for (int cx = x0; cx <= x1; ++cx) {
                BaseBlock* block = map->GetBlock(cx, cy);
                if (!block || block->IsDestroyed() || !block->AllowsChainSpin()) {
                    continue;
                }
                allowed = true;
            }
        }
    }
    if (spin_) {
        spin_->SetSpinAllowed(allowed);
    }
}

void ChainManager::UpdateTether() {
    tetherTaut_ = false;
    if (!player_ || !playerChain_) {
        return;
    }
    // 構え中はスピン側が入力修飾を持つ。それ以外はここで毎フレーム決める（張っていなければ通常）
    if (spin_ && spin_->IsInStance()) {
        return;
    }
    if (!params_.tetherEnabled_ || tornChain_ || transitionHidden_ || player_->IsDead() || player_->IsGoal()) {
        player_->SetActionInputModifier(1.0f, false);
        return;
    }
    float length = playerChain_->GetTotalLength();
    Vector3 gem = playerChain_->GetEndPosition();
    Vector3 hand = lastSocketWorld_;
    float dx = hand.x - gem.x;
    float dy = hand.y - gem.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    bool taut = length > 0.0f && dist > length * (1.0f + params_.tetherSlack_) && dist > 1e-3f;
    tetherTaut_ = taut;

    // デメリットは「本数が多いほどジャンプが低い」の1本に留める。テザーは本数に依存しない手応えだけにする
    // （跳んだ時に鎖の長さで上昇を止めると、短い鎖ほど早く止まり「短い＝身軽」と逆になるので入れない）
    float moveFactor = 1.0f;
    if (taut) {
        // n = 宝石→手（離れる向き）
        float nx = dx / dist;
        float ny = dy / dist;
        Vector3 v = player_->GetVelocity();
        // 横：地上で、宝石を後ろに引きずって離れる向きに歩いている間だけ少し遅くする。近づく向き・空中は自由
        if (player_->IsOnGround() && v.x * nx > 0.0f) {
            moveFactor = params_.dragFactor_;
        }
        // 縦：宝石が自分より上（段の上に残して飛び降りた等）にある時だけ、離れる向きの落下を緩める（ぶら下がり感）
        // 宝石が下にある時（床に残して跳ぶ）は削らない＝ジャンプの罰則は本数のものだけ
        float away = v.y * ny;
        if (ny < 0.0f && away > 0.0f) {
            v.y -= ny * away * params_.tetherPull_;
            player_->SetVelocity(v);
        }
    }
    player_->SetActionInputModifier(moveFactor, false);
}

void ChainManager::UpdateTear(float dt, MapChip2D* map) {
    if (!player_ || !playerChain_) {
        return;
    }
    // 復活した瞬間：ちぎれて落ちていた鎖を消し、手元の鎖は挟まれ固定を解除して手元から垂れた姿勢に作り直す
    // （古い固定フラグが残っていると復活直後にまた「挟まれた」と判定されて無限に死ぬ）
    bool dead = player_->IsDead();
    if (wasDead_ && !dead) {
        ClearTorn();
        playerChain_->ResetToInitial();     // ちぎれで短くなった鎖を初期本数に戻す（個数は Reconcile が合わせる）
        ApplyTreasureParams();              // 外していた末端の重りを戻す
        playerChain_->ResetPoseHanging(lastSocketWorld_, map);
        tearTimer_ = 0.0f;
    }
    wasDead_ = dead;

    if (!params_.tearEnabled_ || dead || player_->IsGoal() || tornChain_ || transitionHidden_) {
        tearTimer_ = 0.0f;
        return;
    }
    // ドア等に挟まれた節があれば、その節の位置で即ちぎれる（引っ張られて伸びるのを待たない）
    int crushed = playerChain_->FindFirstCrushedNode();
    if (crushed >= 0) {
        Tear(crushed);
        return;
    }
    // 剛体拘束中（掲げている・回している）と発射直後のクールダウン中は判定しない（飛んでいる最中の一時的な伸びを拾わない）
    if (spin_ && (spin_->IsInStance() || spin_->GetState() == ChainSpinAction::State::kCooldown)) {
        tearTimer_ = 0.0f;
        return;
    }
    // 伸び：手元から宝石までの直線距離 ÷ 鎖の実長。宝石が地形に引っかかったまま離れると制約が負けて 1 を超える
    // ただし速く動いている最中は制約の反復が追いつかず一時的に伸びるので、宝石がほぼ止まっている（挟まっている）時だけ数える
    Vector3 endVel = playerChain_->GetEndVelocity();
    float endSpeed = std::sqrt(endVel.x * endVel.x + endVel.y * endVel.y);
    bool stretched = playerChain_->GetSpanRatio() > params_.tearStretchRatio_;
    bool stuck = endSpeed < params_.tearStuckSpeed_;
    if (stretched && stuck) {
        tearTimer_ += dt;
    } else {
        tearTimer_ = 0.0f;
    }
    if (tearTimer_ >= params_.tearGraceTime_) {
        Tear(playerChain_->FindMostStretchedSegment() + 1); // 一番伸びた所でちぎれる
    }
}

void ChainManager::Tear(int splitIndex) {
    if (!player_ || !playerChain_) {
        return;
    }
    const auto& all = playerChain_->GetNodes();
    int n = static_cast<int>(all.size());
    if (n < 3) {
        return;
    }
    // 分割点：手元側は 0..k、ちぎれる側は k..末端（k の節を両側に持たせて隙間を作らない）。両側とも2節以上残す
    int k = std::clamp(splitIndex, 1, n - 2);
    Vector3 breakPos = all[k].pos;
    std::vector<VerletNode> tornNodes(all.begin() + k, all.end());
    EndWeight weight = playerChain_->GetEndWeight();

    // 破断エフェクト再生
    if (breakEffect_) {
        breakEffect_->PlayAt(breakPos);
    }

    // 手元側：k までを残す。宝石（末端の重り）はちぎれた側へ移る
    playerChain_->TruncateNodes(k + 1);
    EndWeight none;
    playerChain_->SetEndWeight(none);
    playerChain_->ClearCrushed();

    // ちぎれた側：その場に落ちる（挟まれた節は固定のまま残るので、ドアに刺さったままになる）
    tornChain_ = std::make_unique<Chain2D>();
    tornChain_->InitializeFromNodes(std::move(tornNodes), playerChain_->GetParams(), "TornChain");
    tornChain_->SetEndWeight(weight);
    tornChain_->SetPlayerCollisionEnabled(false);
    tornChain_->ResetDynamics();
    tearTimer_ = 0.0f;
    if (spin_) {
        spin_->Cancel(player_, playerChain_.get());
    }
    if (treasure_) {
        treasure_->SetHighlight(false);
    }
    // ちぎれ＝ミス
    player_->Kill();
    Log("ChainManager: chain torn at node " + std::to_string(k) + " -> miss" + std::string(1, char(10)));
}

void ChainManager::ClearTorn() {
    tornChain_.reset();
    tearTimer_ = 0.0f;
    if (playerChain_) {
        ApplyTreasureParams(); // ちぎれで外していた末端の重りを戻す
    }
}

void ChainManager::SyncTreasureTransform() {
    if (!treasure_ || !playerChain_) {
        return;
    }
    // ちぎれた後は落ちた鎖の先に宝石がある
    const Chain2D* source = tornChain_ ? tornChain_.get() : playerChain_.get();
    int n = source->GetNodeCount();
    if (n < 2) {
        return;
    }
    treasure_->UpdateTransform(source->GetEndPosition(), source->GetNodePosition(n - 2));
}

Vector3 ChainManager::ComputeSocketWorld() {
    socketValid_ = false;
    if (player_) {
        if (Object3D* model = player_->GetModelObject()) {
            if (auto pos = model->GetJointWorldPosition("右手")) {
                socketValid_ = true;
                return *pos;
            }
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
    ImGui::SeparatorText("Tether (weight pulls the player)");
    ImGui::Checkbox("Tether Enabled##Tether", &params_.tetherEnabled_);
    if (ImGui::DragFloat("Drag Factor (引きずりの速度倍率)##Tether", &params_.dragFactor_, 0.01f, 0.0f, 1.0f)) {
        params_.dragFactor_ = std::clamp(params_.dragFactor_, 0.0f, 1.0f);
    }
    if (ImGui::DragFloat("Tether Pull (縦の引き戻し)##Tether", &params_.tetherPull_, 0.01f, 0.0f, 1.0f)) {
        params_.tetherPull_ = std::clamp(params_.tetherPull_, 0.0f, 1.0f);
    }
    if (ImGui::DragFloat("Tether Slack##Tether", &params_.tetherSlack_, 0.005f, 0.0f, 0.5f)) {
        params_.tetherSlack_ = std::clamp(params_.tetherSlack_, 0.0f, 0.5f);
    }
    ImGui::SameLine();
    ImGui::TextDisabled(tetherTaut_ ? "[taut]" : "[slack]");

    ImGui::SeparatorText("Tear (chain snaps = miss)");
    ImGui::Checkbox("Tear Enabled##Tear", &params_.tearEnabled_);
    if (ImGui::DragFloat("Tear Stretch Ratio (直線距離/実長)##Tear", &params_.tearStretchRatio_, 0.01f, 1.05f, 5.0f)) {
        params_.tearStretchRatio_ = std::clamp(params_.tearStretchRatio_, 1.05f, 5.0f);
    }
    if (ImGui::DragFloat("Tear Stuck Speed (宝石がこれ未満で停止扱い)##Tear", &params_.tearStuckSpeed_, 0.1f, 0.0f, 20.0f)) {
        params_.tearStuckSpeed_ = std::clamp(params_.tearStuckSpeed_, 0.0f, 20.0f);
    }
    if (ImGui::DragFloat("Tear Grace Time##Tear", &params_.tearGraceTime_, 0.01f, 0.0f, 2.0f)) {
        params_.tearGraceTime_ = std::clamp(params_.tearGraceTime_, 0.0f, 2.0f);
    }
    if (playerChain_) {
        Vector3 ev = playerChain_->GetEndVelocity();
        ImGui::Text("span ratio %.2f  gem speed %.1f  timer %.2f  %s", playerChain_->GetSpanRatio(),
                    std::sqrt(ev.x * ev.x + ev.y * ev.y), tearTimer_, tornChain_ ? "[TORN]" : "");
    }

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
    ImGui::TextDisabled("W: どこでも宝石を頭上に掲げる → 木の板の上で A/D: その方向へ振り下ろして振り子開始 → A/D で漕ぐ → W を離すと宝石の進行方向へ飛ぶ（上限 = ジャンプ初速 x Ratio）。棒が地形に当たると解除");
    spinChanged |= ImGui::DragFloat("Spin Radius Max##Spin", &params_.spinRadiusMax_, 0.05f, 0.3f, 10.0f);
    spinChanged |= ImGui::DragFloat("Spin Radius Ratio##Spin", &params_.spinRadiusRatio_, 0.01f, 0.3f, 1.0f);
    spinChanged |= ImGui::DragFloat("Hold Offset (掲げる高さ)##Spin", &params_.holdOffset_, 0.01f, 0.05f, 2.0f);
    spinChanged |= ImGui::DragFloat("Throw Out Time (伸び切るまで)##Spin", &params_.throwOutTime_, 0.01f, 0.01f, 2.0f);
    spinChanged |= ImGui::DragFloat("Throw Angle Deg (180=真上から)##Spin", &params_.throwAngleDeg_, 1.0f, 0.0f, 180.0f);
    spinChanged |= ImGui::DragFloat("Throw Omega (投げの角速度)##Spin", &params_.throwOmega_, 0.1f, 0.0f, 20.0f);
    spinChanged |= ImGui::Checkbox("Spin Anywhere (OFF: 木の板の上でのみ回せる)##Spin", &params_.spinAnywhere_);
    if (spin_) {
        ImGui::SameLine();
        ImGui::TextDisabled(spin_->IsSpinAllowed() ? "[on plank]" : "[not on plank]");
    }
    spinChanged |= ImGui::DragFloat("Swing Strength##Spin", &params_.swingStrength_, 0.5f, 0.0f, 200.0f);
    spinChanged |= ImGui::DragFloat("Swing Damping##Spin", &params_.swingDamping_, 0.01f, 0.0f, 5.0f);
    spinChanged |= ImGui::DragFloat("Chain Mass Per Unit##Spin", &params_.chainMassPerUnit_, 0.05f, 0.0f, 10.0f);
    spinChanged |= ImGui::DragFloat("Weight Throw Scale##Spin", &params_.weightThrowScale_, 0.05f, 0.0f, 3.0f);
    spinChanged |= ImGui::DragFloat("Launch Transfer (飛ぶ速さ = 投げた速さ x これ)##Spin", &params_.pullTransfer_, 0.05f, 0.0f, 3.0f);
    spinChanged |= ImGui::DragFloat("Launch Max Jump Ratio##Spin", &params_.launchMaxJumpRatio_, 0.05f, 0.1f, 1.7f);
    spinChanged |= ImGui::DragFloat("Launch Min Upward##Spin", &params_.launchMinUpward_, 0.01f, 0.0f, 1.0f);
    spinChanged |= ImGui::DragFloat("Stance Move Factor##Spin", &params_.spinMoveFactor_, 0.05f, 0.0f, 1.0f);
    spinChanged |= ImGui::DragFloat("Spin Cooldown##Spin", &params_.spinCooldown_, 0.05f, 0.0f, 3.0f);
    if (spinChanged) {
        // ImGuiのキーボード入力で不正値が入っても NaN や壁すり抜けにならないよう、ChainConfig::Load と同じ正規化を行う
        params_.spinRadiusMax_ = (std::max)(0.3f, params_.spinRadiusMax_);
        params_.spinRadiusRatio_ = std::clamp(params_.spinRadiusRatio_, 0.3f, 1.0f);
        params_.holdOffset_ = std::clamp(params_.holdOffset_, 0.05f, 2.0f);
        params_.throwOutTime_ = std::clamp(params_.throwOutTime_, 0.01f, 2.0f);
        params_.throwAngleDeg_ = std::clamp(params_.throwAngleDeg_, 0.0f, 180.0f);
        params_.throwOmega_ = std::clamp(params_.throwOmega_, 0.0f, 20.0f);
        params_.swingStrength_ = (std::max)(0.0f, params_.swingStrength_);
        params_.swingDamping_ = (std::max)(0.0f, params_.swingDamping_);
        params_.chainMassPerUnit_ = (std::max)(0.0f, params_.chainMassPerUnit_);
        params_.weightThrowScale_ = (std::max)(0.0f, params_.weightThrowScale_);
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
