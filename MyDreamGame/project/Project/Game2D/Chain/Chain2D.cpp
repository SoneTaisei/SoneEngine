#include "Chain2D.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Player/Player2D.h"
#include "GameObject/Object3D.h"
#include "Resource/Model/ModelManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Core/Utility/TransformFunctions.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
    constexpr float kPi = std::numbers::pi_v<float>;

    // kusariリンクモデル(kusari_tate/kusari_yoko)の長軸(Z)方向の実寸（OBJ実測値）
    constexpr float kLinkModelLength = 0.468f;

    // アンカーが1フレームでこれ以上動いたらワープとみなして速度をリセットする
    // （リスポーン・部屋遷移で鎖が鞭のように暴れるのを防ぐ）
    constexpr float kTeleportThreshold = 2.0f;

    // 描画時のZオフセット（ブロックより手前に表示する。物理はz=0のまま）
    constexpr float kDrawOffsetZ = -0.2f;

    // 先頭セグメントの自然長がこの割合未満の間は先頭リンクを描かない（極短いリンクのチラつき防止）
    constexpr float kHeadLinkVisibleRatio = 0.3f;

    // モデルの読み込み（ModelManagerがキャッシュするので何度呼んでも安い）
    Model* LoadLinkModel(const char* folderName, const char* fileName) {
        return ModelManager::GetInstance()->GetModel(
            std::string("resources/Object/Original/kusari/") + folderName, fileName);
    }
}

void Chain2D::Initialize(const Vector3& anchorPos, const ChainParams& params, const std::string& name) {
    name_ = name;
    params_ = params;
    anchorPos_ = anchorPos;
    anchorPos_.z = 0.0f;
    initialAnchorPos_ = anchorPos_;
    initialMode_ = anchorMode_;

    device_ = DirectXCommon::GetInstance()->GetDevice();

    // 縦リンク・横リンクを交互に使うことで本物の鎖のような見た目にする
    // 注意: リンクモデルのテクスチャ(white.png)は16x16以上であること
    // （エンジンのテクスチャロードがミップマップ4レベルを生成するため、1x1だとアサートで停止する）
    modelTate_ = LoadLinkModel("kusari_tate", "kusari_tate.obj");
    modelYoko_ = LoadLinkModel("kusari_yoko", "kusari_yoko.obj");

    BuildNodes();

    // 上限ユニット数分のリンクを先に確保しておく（繰り出し中に定数バッファ生成が走らないように）
    int nodesPerUnit = (std::max)(1, params_.nodesPerUnit_);
    size_t capacity = (std::max)(nodes_.size() - 1, static_cast<size_t>((std::max)(1, params_.maxUnits_) * nodesPerUnit));
    EnsureLinkCapacity(capacity);
    UpdateLinkTransforms();
}

void Chain2D::BuildNodes() {
    // ユニット制：節数 = アンカー1個 + ユニット数 × nodesPerUnit_
    int nodesPerUnit = (std::max)(1, params_.nodesPerUnit_);
    int units = (std::max)(1, params_.initialUnits_);
    restLength_ = params_.unitLength_ / static_cast<float>(nodesPerUnit);
    int nodeCount = 1 + units * nodesPerUnit;

    nodes_.clear();
    nodes_.resize(nodeCount);
    for (int i = 0; i < nodeCount; ++i) {
        VerletNode& node = nodes_[i];
        // アンカーから真下に垂らした姿勢で初期化
        node.pos = { anchorPos_.x, anchorPos_.y - restLength_ * static_cast<float>(i), 0.0f };
        node.prevPos = node.pos;
        // 先頭ノードがアンカー（固定）。ただしkFree（落ちている状態）なら全ノード自由
        node.invMass = (i == 0 && anchorMode_ != ChainAnchorMode::kFree) ? 0.0f : 1.0f;
        node.radius = params_.nodeRadius_;
    }

    // 繰り出し状態をクリア（先頭セグメントは完全長）
    pendingNodes_ = 0;
    headRest_ = restLength_;

    // 終端拘束が設定されている場合は復元
    if (endFollow_ && !nodes_.empty()) {
        nodes_.back().invMass = 0.0f;
    }
    // 末端の重りを再適用（ノード再構築で invMass が 1 に戻るため）
    ApplyEndWeight();
}

void Chain2D::EnsureLinkCapacity(size_t count) {
    if (!device_) {
        return;
    }
    // 添字 j = 末端からの距離。末端リンク(j=0)が縦、以降交互。手元側で増減しても既存リンクのモデルは変わらない
    while (linkObjs_.size() < count) {
        size_t j = linkObjs_.size();
        auto obj = std::make_unique<Object3D>();
        Model* model = (j % 2 == 0) ? modelTate_ : modelYoko_;
        obj->Initialize(device_, model);
        obj->SetName(name_ + "_Link" + std::to_string(j));
        obj->GetMaterial().lightingType = 1;
        linkObjs_.push_back(std::move(obj));
    }
}

void Chain2D::Update(float dt, MapChip2D* map, Player2D* player) {
    if (nodes_.size() < 2 || dt <= 0.0f) {
        return;
    }

    // 1. アンカー更新（kFree=落ちている状態なら固定しない）
    if (anchorMode_ != ChainAnchorMode::kFree) {
        Vector3 newAnchor = anchorFollow_ ? *anchorFollow_ : anchorPos_;
        newAnchor.z = 0.0f;

        // ワープ検出：アンカーが瞬間移動した場合は全ノードの速度をリセットして暴れを防ぐ
        // （リスポーン・部屋遷移・拾った瞬間の遠距離ジャンプを全部ここで吸収）
        Vector3 diff = newAnchor - nodes_[0].pos;
        float distSq = diff.x * diff.x + diff.y * diff.y;
        bool teleported = distSq > kTeleportThreshold * kTeleportThreshold;

        anchorPos_ = newAnchor;
        nodes_[0].invMass = 0.0f;
        nodes_[0].pos = newAnchor;
        nodes_[0].prevPos = newAnchor;

        if (teleported) {
            ResetDynamics();
        }
    } else {
        nodes_[0].invMass = 1.0f;
    }

    // 2. 終端拘束（プレイヤーが掴んだ状態などの将来拡張用）
    if (endFollow_) {
        VerletNode& end = nodes_.back();
        Vector3 target = *endFollow_;
        target.z = 0.0f;
        end.invMass = 0.0f;
        end.pos = target;
        end.prevPos = target;
    }

    // 3. 手元からの繰り出し（ノード数が変わるので末端の重りも再適用する）
    UpdatePayout(dt);
    ApplyEndWeight();

    int subSteps = (std::max)(1, params_.subSteps_);
    float subDt = dt / static_cast<float>(subSteps);

    // 3.5 剛体棒モード：全ノードをアンカー→目標の直線上に置き直して物理は止める
    // （振り回し中に途中の鎖がたるんで重りが勝手に動いて見えるのを防ぐ。ピンと張ったまま回る）
    if (rigidLine_) {
        Vector3 target = *rigidLine_;
        target.z = 0.0f;
        PlaceNodesOnLine(nodes_[0].pos, target);
        lastStepDt_ = subDt;
        VerletPhysics2D::ClampToPlaneZ(nodes_);
        UpdateLinkTransforms();
        return;
    }

    // 4. サブステップ分割（硬さが欲しい時は反復を増やすよりこちらが同コストで効く）
    for (int s = 0; s < subSteps; ++s) {
        StepSimulation(subDt, map, player);
    }

    // 5. z成分を0に矯正（誤差蓄積の保険）
    VerletPhysics2D::ClampToPlaneZ(nodes_);

    // 6. 描画用モデルの姿勢更新
    UpdateLinkTransforms();
}

void Chain2D::UpdatePayout(float dt) {
    float feed = params_.payoutSpeed_ * dt;

    if (pendingNodes_ <= 0) {
        // キューが空でも、最後に挿入した節が伸び切るまで先頭セグメントを伸ばし続ける
        headRest_ = (std::min)(headRest_ + feed, restLength_);
        return;
    }

    // 先頭セグメントの自然長を伸ばし、1節分に達するたびに手元へノードを1個挿入する
    // （挿入位置はアンカーそのもの・速度ゼロ。headRest_ が小さい間は手の中に留まり、伸びるにつれて出てくる）
    headRest_ += feed;
    while (pendingNodes_ > 0 && headRest_ >= restLength_) {
        VerletNode node;
        node.pos = nodes_[0].pos;
        node.prevPos = node.pos;
        node.invMass = 1.0f;
        node.radius = params_.nodeRadius_;
        nodes_.insert(nodes_.begin() + 1, node);
        headRest_ -= restLength_;
        --pendingNodes_;
    }
    if (pendingNodes_ <= 0) {
        headRest_ = (std::min)(headRest_, restLength_);
    }
}

void Chain2D::ApplyEndWeight() {
    // アンカーしか無い場合は対象外
    if (nodes_.size() < 2) {
        return;
    }
    VerletNode& end = nodes_.back();
    if (endWeight_.enabled) {
        end.radius = endWeight_.radius;
        if (!endFollow_) {
            end.invMass = 1.0f / (std::max)(0.1f, endWeight_.mass);
        }
    } else {
        end.radius = params_.nodeRadius_;
        if (!endFollow_) {
            end.invMass = 1.0f;
        }
    }
}

void Chain2D::StepSimulation(float dt, MapChip2D* map, Player2D* player) {
    lastStepDt_ = (dt > 0.0f) ? dt : lastStepDt_;

    // Verlet積分
    Vector3 gravity = { 0.0f, params_.gravity_, 0.0f };
    VerletPhysics2D::Integrate(nodes_, gravity, params_.damping_, dt);

    // プレイヤーのAABB（死亡中は判定しない。playerCollisionEnabled_=false の鎖＝プレイヤーが持っている鎖は判定しない）
    // 判定する場合、手持ち中（kSocket）は根元 playerCollisionSkip_ ノードだけ除外して残りは判定する
    // （根元がプレイヤーに追従するため、除外しないと毎フレーム押し合う）
    bool isHeld = (anchorMode_ == ChainAnchorMode::kSocket);
    bool hitPlayer = player && !player->IsDead() && playerCollisionEnabled_;
    size_t playerSkip = isHeld ? static_cast<size_t>((std::max)(0, playerCollisionSkip_)) : 0;
    AABB2D playerBox{};
    if (hitPlayer) {
        playerBox = player->GetAABB();
    }

    // 手に持っている間は根元の数ノードを地形判定から除外する
    // （壁張り付き時にソケットがブロック内部へ入り得るため、ジッタ防止）
    size_t rootSkip = 0;
    if (anchorMode_ == ChainAnchorMode::kSocket) {
        rootSkip = static_cast<size_t>((std::max)(0, params_.rootCollisionSkip_));
    }

    // 末端（お宝）の扱い
    const size_t lastIndex = nodes_.size() - 1;
    const bool endIsWeight = endWeight_.enabled;
    const bool endSkipsPlayer = endIsWeight && endWeight_.ignorePlayer;

    // 反復ループ：距離制約とコリジョンを同一ループ内で交互に解いて同時収束させる
    // （別々に1回ずつだと制約が押し出しを壊し、押し出しが距離を壊す）
    int iterations = (std::max)(1, params_.iterations_);
    for (int iter = 0; iter < iterations; ++iter) {
        // 距離制約（先頭セグメントだけ繰り出し中の自然長 headRest_ を使う）
        for (size_t i = 0; i + 1 < nodes_.size(); ++i) {
            float rest = (i == 0) ? headRest_ : restLength_;
            VerletPhysics2D::SolveDistanceConstraint(nodes_[i], nodes_[i + 1], rest);
        }
        // チップ押し出し（反復中は摩擦なし。摩擦は最後のパスでのみ適用する）
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (i >= rootSkip) {
                VerletPhysics2D::CollideNodeWithMap(nodes_[i], map, 0.0f);
            }
            if (hitPlayer && i >= playerSkip && !(endSkipsPlayer && i == lastIndex)) {
                VerletPhysics2D::CollideNodeWithAABB(nodes_[i], playerBox, 0.0f);
            }
        }
    }

    // 最後にもう1回押し出しパス（ここでのみ摩擦・速度伝搬を適用）
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (i >= rootSkip) {
            // 末端の重りは専用の摩擦（引きずると渋い）
            float mapFriction = (endIsWeight && i == lastIndex) ? endWeight_.friction : params_.friction_;
            VerletPhysics2D::CollideNodeWithMap(nodes_[i], map, mapFriction);
        }
        if (hitPlayer && i >= playerSkip && !(endSkipsPlayer && i == lastIndex)) {
            if (VerletPhysics2D::CollideNodeWithAABB(nodes_[i], playerBox, params_.friction_)) {
                // 接触したノードへプレイヤーの速度を伝える（ダッシュで駆け抜けると跳ね上がる）
                // 手持ち中は根元がプレイヤー追従のためエネルギーが増幅しやすく、速度伝搬は行わない
                if (!isHeld) {
                    VerletPhysics2D::ApplyVelocity(nodes_[i], player->GetVelocity(), dt, params_.playerVelInfluence_);
                }
            }
        }
    }
}

void Chain2D::UpdateLinkTransforms() {
    size_t linkCount = (nodes_.size() >= 2) ? nodes_.size() - 1 : 0;
    EnsureLinkCapacity(linkCount);
    if (linkObjs_.size() < linkCount) {
        return; // device 未設定など
    }

    // リンクモデルの長さ・太さ（太さは完全長基準で一定にする）
    float fullScale = (restLength_ * params_.linkOverlap_) / kLinkModelLength;
    float thickness = fullScale * params_.linkThickness_;

    hideHeadLink_ = false;
    for (size_t i = 0; i < linkCount; ++i) {
        const Vector3& p1 = nodes_[i].pos;
        const Vector3& p2 = nodes_[i + 1].pos;

        Vector3 mid = (p1 + p2) * 0.5f;
        mid.z = kDrawOffsetZ;
        float angle = std::atan2(p2.y - p1.y, p2.x - p1.x);

        // 先頭セグメントは繰り出し中の自然長に合わせて短く描く（手から生えてくる見た目）
        float segRest = (i == 0) ? headRest_ : restLength_;
        float lengthScale = (segRest * params_.linkOverlap_) / kLinkModelLength;
        if (i == 0 && headRest_ < restLength_ * kHeadLinkVisibleRatio) {
            hideHeadLink_ = true;
        }

        // 添字 j は末端からの距離（パリティを末端基準にして手元での増減でチカチカしないようにする）
        size_t j = linkCount - 1 - i;
        Object3D* link = linkObjs_[j].get();
        link->SetTranslation(mid);
        // モデルの長軸はZ方向なので、X軸-90度で画面内(Y方向)へ倒してからZ回転で節方向に合わせる
        // (回転合成順 Rx→Ry→Rz の行ベクトル規約で、長軸は (-sinθ, cosθ) を向くため -π/2 補正)
        link->SetRotation({ -kPi * 0.5f, 0.0f, angle - kPi * 0.5f });
        link->SetScale({ thickness, thickness, lengthScale });
    }
}

void Chain2D::Draw() {
    size_t linkCount = (nodes_.size() >= 2) ? nodes_.size() - 1 : 0;
    if (linkObjs_.size() < linkCount) {
        return;
    }
    for (size_t j = 0; j < linkCount; ++j) {
        // 先頭リンク(j = linkCount-1)は繰り出し直後の極短い間だけ描かない
        if (hideHeadLink_ && j == linkCount - 1) {
            continue;
        }
        linkObjs_[j]->Draw();
    }
}

void Chain2D::ResetDynamics() {
    VerletPhysics2D::ResetVelocities(nodes_);
}

void Chain2D::ResetToInitial() {
    // 初期モード・初期アンカーに戻して垂下姿勢を再構築する（繰り出し状態もクリアされる）
    anchorMode_ = initialMode_;
    anchorPos_ = initialAnchorPos_;
    BuildNodes();
    UpdateLinkTransforms();
}

void Chain2D::SetAnchorMode(ChainAnchorMode mode) {
    anchorMode_ = mode;
    if (!nodes_.empty()) {
        nodes_[0].invMass = (mode == ChainAnchorMode::kFree) ? 1.0f : 0.0f;
    }
}

void Chain2D::SyncSocket(const Vector3& socketWorld) {
    // シミュレーションは z=0 平面で行う
    Vector3 p = socketWorld;
    p.z = 0.0f;
    anchorPos_ = p; // Update() 冒頭のピン留め・ワープ検出がこの値を使う
}

int Chain2D::GetUnitCount() const {
    int nodesPerUnit = (std::max)(1, params_.nodesPerUnit_);
    // 繰り出し待ちも含めて数える（含めないと Reconcile が毎フレーム「足りない」と判断して無限に伸びる）
    return (static_cast<int>(nodes_.size()) - 1 + pendingNodes_) / nodesPerUnit;
}

void Chain2D::SetUnitCount(int units) {
    units = (std::max)(1, units);
    int current = GetUnitCount();
    if (units > current) {
        QueueUnits(units - current);
    } else if (units < current) {
        RemoveUnitsAtAnchor(current - units); // 戻り値は捨てる（外す操作は別経路）
    }
}

void Chain2D::QueueUnits(int units) {
    if (units <= 0) {
        return;
    }
    pendingNodes_ += units * (std::max)(1, params_.nodesPerUnit_);
}

bool Chain2D::IsPayingOut() const {
    return pendingNodes_ > 0 || headRest_ < restLength_ - 1e-4f;
}

void Chain2D::AddUnitsAtAnchor(int units) {
    if (units <= 0 || nodes_.size() < 2) {
        return;
    }
    int insertCount = units * (std::max)(1, params_.nodesPerUnit_);

    // アンカーと隣ノードの間に線形補間で挿入する
    // （全部アンカーに重ねると距離制約が一気に弾いてポップする）
    const Vector3 a = nodes_[0].pos;
    const Vector3 b = nodes_[1].pos;
    std::vector<VerletNode> inserted(insertCount);
    for (int i = 0; i < insertCount; ++i) {
        float t = static_cast<float>(i + 1) / static_cast<float>(insertCount + 1);
        VerletNode& node = inserted[i];
        node.pos = TransformFunctions::Lerp(a, b, t);
        node.prevPos = node.pos; // 速度ゼロで生む
        node.invMass = 1.0f;
        node.radius = params_.nodeRadius_;
    }
    nodes_.insert(nodes_.begin() + 1, inserted.begin(), inserted.end());

    ApplyEndWeight();
    UpdateLinkTransforms();
}

std::vector<VerletNode> Chain2D::RemoveUnitsAtAnchor(int units) {
    std::vector<VerletNode> removed;
    if (units <= 0) {
        return removed;
    }
    int nodesPerUnit = (std::max)(1, params_.nodesPerUnit_);
    int nodesToRemove = units * nodesPerUnit;

    // まだ繰り出していないノードを先に消費する（見えていない分は静かに減らす）
    int fromPending = (std::min)(nodesToRemove, pendingNodes_);
    pendingNodes_ -= fromPending;
    nodesToRemove -= fromPending;
    if (nodesToRemove <= 0 || nodes_.size() < 2) {
        return removed;
    }

    // アンカーノードと、重りが有効なら末端ノードも必ず残す
    int keep = endWeight_.enabled ? 2 : 1;
    int removeCount = (std::min)(nodesToRemove, static_cast<int>(nodes_.size()) - keep);
    if (removeCount <= 0) {
        return removed;
    }

    // アンカーの次から removeCount 個を pos/prevPos そのままで切り離す（落下が連続的に見える）
    removed.assign(nodes_.begin() + 1, nodes_.begin() + 1 + removeCount);
    // 先頭にアンカーの複製を足して「1 + 削除ノード数」の完全なユニット構成にする
    // （これがないと切り離した鎖が1セグメント分短くなり、GetUnitCountも1少なく数えてしまう）
    removed.insert(removed.begin(), nodes_[0]);
    nodes_.erase(nodes_.begin() + 1, nodes_.begin() + 1 + removeCount);
    for (auto& node : removed) {
        node.invMass = 1.0f; // 自由ノード化
        node.radius = params_.nodeRadius_;
    }
    // 残った新しい先頭セグメントは完全長。残りの揺れは殺さない（ResetDynamicsは呼ばない）
    headRest_ = restLength_;

    ApplyEndWeight();
    UpdateLinkTransforms();
    return removed;
}

void Chain2D::InitializeFromNodes(std::vector<VerletNode>&& nodes, const ChainParams& params, const std::string& name) {
    name_ = name;
    params_ = params;

    device_ = DirectXCommon::GetInstance()->GetDevice();
    modelTate_ = LoadLinkModel("kusari_tate", "kusari_tate.obj");
    modelYoko_ = LoadLinkModel("kusari_yoko", "kusari_yoko.obj");

    nodes_ = std::move(nodes);
    for (auto& node : nodes_) {
        node.invMass = 1.0f;
    }

    // 落ちている状態として生成（重りなし・繰り出しなし）
    anchorMode_ = ChainAnchorMode::kFree;
    initialMode_ = ChainAnchorMode::kFree;
    endWeight_ = EndWeight{};
    restLength_ = params_.unitLength_ / static_cast<float>((std::max)(1, params_.nodesPerUnit_));
    pendingNodes_ = 0;
    headRest_ = restLength_;
    anchorPos_ = nodes_.empty() ? Vector3{ 0.0f, 0.0f, 0.0f } : nodes_[0].pos;
    initialAnchorPos_ = anchorPos_;

    // 自由鎖は伸びないので必要本数だけ確保する
    UpdateLinkTransforms();
}

bool Chain2D::FindNearestNodeToAABB(const AABB2D& box, float margin, int* outIndex) const {
    float bestDistSq = -1.0f;
    int bestIndex = -1;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        const Vector3& p = nodes_[i].pos;
        // AABB上の最近接点からノードまでの距離で判定する
        float closestX = std::clamp(p.x, box.left, box.right);
        float closestY = std::clamp(p.y, box.bottom, box.top);
        float dx = p.x - closestX;
        float dy = p.y - closestY;
        float distSq = dx * dx + dy * dy;
        float reach = margin + nodes_[i].radius;
        if (distSq <= reach * reach && (bestIndex < 0 || distSq < bestDistSq)) {
            bestDistSq = distSq;
            bestIndex = static_cast<int>(i);
        }
    }
    if (bestIndex < 0) {
        return false;
    }
    if (outIndex) {
        *outIndex = bestIndex;
    }
    return true;
}

bool Chain2D::FindNearestNode(const Vector3& point, float radius, int* outIndex) const {
    float bestDistSq = radius * radius;
    int bestIndex = -1;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        Vector3 d = nodes_[i].pos - point;
        float distSq = d.x * d.x + d.y * d.y;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIndex = static_cast<int>(i);
        }
    }
    if (bestIndex < 0) {
        return false;
    }
    if (outIndex) {
        *outIndex = bestIndex;
    }
    return true;
}

void Chain2D::SetEndFollowTarget(const Vector3* target) {
    endFollow_ = target;
    if (!nodes_.empty()) {
        // 拘束解除時は終端を自由ノード（重りが有効なら重り）に戻す
        nodes_.back().invMass = endFollow_ ? 0.0f : 1.0f;
    }
    ApplyEndWeight();
}

void Chain2D::ApplyEndVelocity(const Vector3& velocity, float dt) {
    if (nodes_.size() < 2) {
        return;
    }
    // 拘束解除後（invMass > 0）に呼ぶこと。拘束中は VerletPhysics2D 側で無視される
    // Verlet の暗黙速度は「1積分ステップあたりの変位」なので、サブステップ分割時は subDt で注入する
    // （フレーム dt のまま入れると subSteps_ 倍の速さで飛んでしまう）
    float subDt = dt / static_cast<float>((std::max)(1, params_.subSteps_));
    VerletPhysics2D::ApplyVelocity(nodes_.back(), velocity, subDt, 1.0f);
}

void Chain2D::PlaceNodesOnLine(const Vector3& from, const Vector3& to) {
    size_t n = nodes_.size();
    if (n == 0) {
        return;
    }
    Vector3 d = to - from;
    float denom = static_cast<float>((std::max)(static_cast<size_t>(1), n - 1));
    for (size_t i = 0; i < n; ++i) {
        float t = static_cast<float>(i) / denom;
        Vector3 p = { from.x + d.x * t, from.y + d.y * t, 0.0f };
        nodes_[i].pos = p;
        nodes_[i].prevPos = p;
        nodes_[i].invMass = 0.0f; // 固定（地形・プレイヤー判定も自動的にスキップされる）
    }
}

void Chain2D::RestoreMasses() {
    for (auto& node : nodes_) {
        node.invMass = 1.0f;
    }
    if (!nodes_.empty()) {
        nodes_[0].invMass = (anchorMode_ == ChainAnchorMode::kFree) ? 1.0f : 0.0f;
        if (endFollow_) {
            nodes_.back().invMass = 0.0f;
        }
    }
    ApplyEndWeight(); // 末端の重り（invMass = 1/mass）を再適用
}

void Chain2D::SetRigidLineTarget(const Vector3* target) {
    if (rigidLine_ && !target) {
        // 解除：速度ゼロで物理に戻す（prevPos は PlaceNodesOnLine で pos と同じ）
        rigidLine_ = nullptr;
        RestoreMasses();
        return;
    }
    rigidLine_ = target;
}

void Chain2D::ReleaseRigidLine(const Vector3& center, float omega, float velocityScale, float dt) {
    rigidLine_ = nullptr;
    RestoreMasses();
    if (nodes_.size() < 2) {
        return;
    }
    // 剛体回転の速度場 v = ω × r（2Dでは ω(-ry, rx)）を各ノードに与える。サブステップ分割時は subDt で注入する
    float subDt = dt / static_cast<float>((std::max)(1, params_.subSteps_));
    float w = omega * velocityScale;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        VerletNode& node = nodes_[i];
        node.prevPos = node.pos; // 拘束中の残留変位を消してから注入
        if (node.invMass <= 0.0f) {
            continue; // アンカー等の固定ノード
        }
        float rx = node.pos.x - center.x;
        float ry = node.pos.y - center.y;
        Vector3 v = { -ry * w, rx * w, 0.0f };
        VerletPhysics2D::ApplyVelocity(node, v, subDt, 1.0f);
    }
}

Vector3 Chain2D::GetEndVelocity() const {
    if (nodes_.empty() || lastStepDt_ <= 0.0f) {
        return { 0.0f, 0.0f, 0.0f };
    }
    const VerletNode& end = nodes_.back();
    Vector3 d = end.pos - end.prevPos;
    return { d.x / lastStepDt_, d.y / lastStepDt_, 0.0f };
}

float Chain2D::GetTotalLength() const {
    if (nodes_.size() < 2) {
        return 0.0f;
    }
    return headRest_ + restLength_ * static_cast<float>(nodes_.size() - 2);
}

void Chain2D::SetEndWeight(const EndWeight& weight) {
    endWeight_ = weight;
    ApplyEndWeight();
}

void Chain2D::SetParams(const ChainParams& params) {
    bool needRebuild =
        params.initialUnits_ != params_.initialUnits_ ||
        params.nodesPerUnit_ != params_.nodesPerUnit_ ||
        params.unitLength_ != params_.unitLength_;

    params_ = params;
    // ImGui のキーボード入力や外部からの直接指定で不正値が入っても繰り出しが止まらないよう正規化する
    // （payoutSpeed_=0 だと繰り出しが永久に終わらず外す操作が拒否され続ける。ChainConfig::Load と同じ規則）
    params_.nodesPerUnit_ = (std::max)(2, params_.nodesPerUnit_);
    params_.initialUnits_ = (std::max)(1, params_.initialUnits_);
    params_.unitLength_ = (std::max)(0.1f, params_.unitLength_);
    params_.payoutSpeed_ = (std::max)(0.1f, params_.payoutSpeed_);

    if (needRebuild) {
        BuildNodes();
    } else {
        restLength_ = params_.unitLength_ / static_cast<float>((std::max)(1, params_.nodesPerUnit_));
        headRest_ = (std::min)(headRest_, restLength_);
        for (auto& node : nodes_) {
            node.radius = params_.nodeRadius_;
        }
        ApplyEndWeight();
    }
    UpdateLinkTransforms();
}

std::vector<Object3D*> Chain2D::GetLinkObjects() const {
    std::vector<Object3D*> result;
    size_t linkCount = (nodes_.size() >= 2) ? nodes_.size() - 1 : 0;
    linkCount = (std::min)(linkCount, linkObjs_.size());
    result.reserve(linkCount);
    for (size_t j = 0; j < linkCount; ++j) {
        result.push_back(linkObjs_[j].get());
    }
    return result;
}

void Chain2D::DrawImGui() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode(name_.c_str())) {
        // 現在の保持状態
        const char* modeNames[] = { "World (固定)", "Socket (手に追従)", "Free (落下中)" };
        ImGui::Text("Mode: %s", modeNames[static_cast<int>(anchorMode_)]);
        ImGui::Text("Nodes: %d  Pending: %d  HeadRest: %.3f / %.3f",
                    static_cast<int>(nodes_.size()), pendingNodes_, headRest_, restLength_);
        if (endWeight_.enabled) {
            ImGui::Text("EndWeight: mass %.1f  radius %.2f  friction %.2f",
                        endWeight_.mass, endWeight_.radius, endWeight_.friction);
        }

        // アンカー位置
        float anchor[2] = { anchorPos_.x, anchorPos_.y };
        if (ImGui::DragFloat2("Anchor", anchor, 0.1f)) {
            anchorPos_ = { anchor[0], anchor[1], 0.0f };
        }

        // 実行中のユニット数（増える分は繰り出し、減る分はアンカー側から削除）
        int units = GetUnitCount();
        if (ImGui::DragInt("Units", &units, 1, 1, 16)) {
            SetUnitCount(units);
        }

        // パラメータ（ユニット構成の変更はノード再構築が必要なのでSetParams経由）
        ChainParams edit = params_;
        bool changed = false;
        changed |= ImGui::DragInt("Initial Units", &edit.initialUnits_, 1, 1, 16);
        changed |= ImGui::DragFloat("Unit Length", &edit.unitLength_, 0.05f, 0.25f, 3.0f);
        changed |= ImGui::DragInt("Nodes Per Unit", &edit.nodesPerUnit_, 1, 2, 8);
        changed |= ImGui::DragFloat("Payout Speed", &edit.payoutSpeed_, 0.1f, 0.5f, 30.0f);
        changed |= ImGui::DragFloat("Pickup Radius", &edit.pickupRadius_, 0.05f, 0.1f, 3.0f);
        changed |= ImGui::DragFloat("Gravity", &edit.gravity_, 0.5f, -100.0f, 0.0f);
        changed |= ImGui::DragFloat("Damping", &edit.damping_, 0.001f, 0.90f, 1.0f);
        changed |= ImGui::DragInt("Iterations", &edit.iterations_, 1, 1, 50);
        changed |= ImGui::DragInt("Sub Steps", &edit.subSteps_, 1, 1, 8);
        changed |= ImGui::DragFloat("Node Radius", &edit.nodeRadius_, 0.01f, 0.02f, 0.5f);
        changed |= ImGui::DragFloat("Friction", &edit.friction_, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Player Vel Influence", &edit.playerVelInfluence_, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::DragInt("Root Collision Skip", &edit.rootCollisionSkip_, 1, 0, 6);
        changed |= ImGui::DragFloat("Link Thickness", &edit.linkThickness_, 0.01f, 0.2f, 3.0f);
        changed |= ImGui::DragFloat("Link Overlap", &edit.linkOverlap_, 0.01f, 1.0f, 2.5f);
        if (changed) {
            SetParams(edit);
        }

        if (ImGui::Button("Reset Pose")) {
            ResetToInitial();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Params")) {
            ChainConfig::Save(params_, ChainConfig::kDefaultFilePath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Params")) {
            ChainParams loaded = params_;
            ChainConfig::Load(loaded, ChainConfig::kDefaultFilePath);
            SetParams(loaded);
        }

        ImGui::TreePop();
    }
#endif
}
