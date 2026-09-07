#include "TransitionDirector.h"
#include "Game2D/Chain/ChainManager.h"
#include "Game2D/Treasure/Treasure2D.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Player/Player2D.h"
#include "Graphics/GameCamera.h"
#include "GameObject/Object3D.h"
#include "Resource/Model/ModelManager.h"
#include "Renderer/DirectXCommon/DirectXCommon.h"
#include "Core/Utility/ParameterManager.h"
#include "Core/Utility/UtilityFunctions.h"
#include "Core/Utility/BlendMode.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
    constexpr const char* kParamGroup = "Transition";
    constexpr const char* kIrisModelDir = "resources/Object/Original/iris";
    constexpr const char* kIrisModelFile = "iris_ring.obj";
    constexpr const char* kHandJointName = "RightHand_Dummy_017";
    constexpr float kPi = std::numbers::pi_v<float>;
    // 固定ステップ（速度注入用。TimeManager と同じ 1/60）
    constexpr float kFixedDt = 1.0f / 60.0f;
    // 宝石は鎖よりこれだけ手前に描く
    constexpr float kTreasureZBias = -0.05f;
    // 飛び抜ける時の鎖と宝石は本人よりこれだけ手前に描く
    constexpr float kFlyChainZBias = -0.2f;
    // 喜ぶ時の伸び縮み
    constexpr float kHopSquash = 0.12f;

    float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
    float Lerp(float a, float b, float t) { return a + (b - a) * t; }
    Vector3 Lerp(const Vector3& a, const Vector3& b, float t) { return a + (b - a) * t; }
    float EaseIn(float u) { return u * u; }
    float EaseOut(float u) { return 1.0f - (1.0f - u) * (1.0f - u); }
    float EaseInOut(float u) {
        return (u < 0.5f) ? 2.0f * u * u : 1.0f - (-2.0f * u + 2.0f) * (-2.0f * u + 2.0f) * 0.5f;
    }
    float SafeTime(float t) { return (std::max)(0.001f, t); }

    Object3D* MakeBlackRing(Model* model, const char* name) {
        auto* obj = new Object3D();
        obj->Initialize(DirectXCommon::GetInstance()->GetDevice(), model);
        obj->SetName(name);
        obj->SetIsDoubleSided(true); // 法線の向きを気にしない
        obj->SetBlendMode(BlendMode::kBlendModeNormal);
        Material& mat = obj->GetMaterial();
        mat.color = { 0.0f, 0.0f, 0.0f, 1.0f };
        mat.lightingType = 0;
        mat.enableEnvironmentMap = 0;
        return obj;
    }
}

TransitionDirector* TransitionDirector::GetInstance() {
    static TransitionDirector instance;
    return &instance;
}

TransitionDirector::~TransitionDirector() = default;

void TransitionDirector::LoadParams() {
    ParameterManager* pm = ParameterManager::GetInstance();
    params_.clearStyle_ = pm->GetValue(kParamGroup, "clearStyle", 0);
    params_.zoomTime_ = pm->GetValue(kParamGroup, "zoomTime", 0.7f);
    params_.zoomScale_ = pm->GetValue(kParamGroup, "zoomScale", 2.2f);
    params_.joyTime_ = pm->GetValue(kParamGroup, "joyTime", 1.2f);
    params_.hopHeight_ = pm->GetValue(kParamGroup, "hopHeight", 0.6f);
    params_.hopCount_ = pm->GetValue(kParamGroup, "hopCount", 2);
    params_.joySpin_ = pm->GetValue(kParamGroup, "joySpin", true);
    params_.enterGoalTime_ = pm->GetValue(kParamGroup, "enterGoalTime", 0.7f);
    params_.enterGoalYaw_ = pm->GetValue(kParamGroup, "enterGoalYaw", 3.14159f);
    params_.enterGoalDepth_ = pm->GetValue(kParamGroup, "enterGoalDepth", 1.5f);
    params_.flyTime_ = pm->GetValue(kParamGroup, "flyTime", 1.1f);
    params_.flyScale_ = pm->GetValue(kParamGroup, "flyScale", 3.0f);
    params_.flyZ_ = pm->GetValue(kParamGroup, "flyZ", -2.0f);
    params_.flyTilt_ = pm->GetValue(kParamGroup, "flyTilt", 0.6f);
    params_.flyArcHeight_ = pm->GetValue(kParamGroup, "flyArcHeight", 1.0f);
    params_.flyMargin_ = pm->GetValue(kParamGroup, "flyMargin", 2.5f);
    params_.irisCloseTime_ = pm->GetValue(kParamGroup, "irisCloseTime", 0.6f);
    params_.irisHoldRadius_ = pm->GetValue(kParamGroup, "irisHoldRadius", 1.5f);
    params_.holdTime_ = pm->GetValue(kParamGroup, "holdTime", 0.2f);
    params_.descendTime_ = pm->GetValue(kParamGroup, "descendTime", 0.8f);
    params_.irisFinishTime_ = pm->GetValue(kParamGroup, "irisFinishTime", 0.2f);
    params_.landTime_ = pm->GetValue(kParamGroup, "landTime", 0.8f);
    params_.irisOpenTime_ = pm->GetValue(kParamGroup, "irisOpenTime", 0.8f);
    params_.irisZ_ = pm->GetValue(kParamGroup, "irisZ", -0.9f);
    params_.backdropZ_ = pm->GetValue(kParamGroup, "backdropZ", -0.6f);
    params_.carryZ_ = pm->GetValue(kParamGroup, "carryZ", -0.8f);
    params_.irisMinScale_ = pm->GetValue(kParamGroup, "irisMinScale", 0.003f);
    params_.descendMargin_ = pm->GetValue(kParamGroup, "descendMargin", 0.5f);
    params_.maxAnchorAccelRatio_ = pm->GetValue(kParamGroup, "maxAnchorAccelRatio", 0.9f);
    params_.carryChainLength_ = pm->GetValue(kParamGroup, "carryChainLength", true);

    // 手編集で壊れた値の保険（前後関係: 黒板 < 鎖 < 背景板 < ブロック前面(-0.5)）
    params_.clearStyle_ = std::clamp(params_.clearStyle_, 0, 1);
    params_.zoomScale_ = std::clamp(params_.zoomScale_, 1.0f, 6.0f);
    params_.hopCount_ = std::clamp(params_.hopCount_, 1, 6);
    params_.flyScale_ = std::clamp(params_.flyScale_, 0.5f, 8.0f);
    params_.flyZ_ = std::clamp(params_.flyZ_, -8.0f, -1.0f);
    params_.irisHoldRadius_ = (std::max)(0.2f, params_.irisHoldRadius_);
    params_.irisMinScale_ = std::clamp(params_.irisMinScale_, 0.001f, 0.05f);
    params_.maxAnchorAccelRatio_ = std::clamp(params_.maxAnchorAccelRatio_, 0.1f, 5.0f);
    params_.carryZ_ = (std::min)(params_.carryZ_, -0.7f);
    params_.backdropZ_ = std::clamp(params_.backdropZ_, params_.carryZ_ + 0.1f, -0.55f);
    params_.irisZ_ = (std::min)(params_.irisZ_, params_.carryZ_ - 0.05f);
}

void TransitionDirector::EnsureObjects() {
    if (iris_ && backdrop_) {
        return;
    }
    Model* model = ModelManager::GetInstance()->GetModel(kIrisModelDir, kIrisModelFile);
    if (!model) {
        Log("TransitionDirector: iris model not found\n");
        return;
    }
    if (!iris_) {
        iris_.reset(MakeBlackRing(model, "TransitionIris"));
    }
    if (!backdrop_) {
        // 背景板は穴を閉じ切ったリング = 黒い板。中心は穴と同じ位置に置く（針穴は宝石に隠れる）
        backdrop_.reset(MakeBlackRing(model, "TransitionBackdrop"));
    }
}

// ---------------------------------------------------------------------------
// 持ち越し用の鎖
// ---------------------------------------------------------------------------

void TransitionDirector::CloneCarry(ChainManager* chains) {
    Chain2D* src = chains->GetPlayerChain();
    const ChainParams& p = src->GetParams();

    // 次シーンで作り直せるように控える
    carryParams_ = p;
    carryWeight_ = src->GetEndWeight();
    carryUnits_ = (std::max)(1, src->GetUnitCount());
    hasCarryData_ = true;

    // ノード列をそのまま複製（位置・速度が連続するので切り替えは見えない）
    std::vector<VerletNode> nodes = src->GetNodes();
    Vector3 anchor = nodes.empty() ? src->GetAnchorPosition() : nodes[0].pos;

    carryChain_ = std::make_unique<Chain2D>();
    carryChain_->SetDrawOffsetZ(params_.carryZ_); // 背景板より手前に描く
    carryChain_->InitializeFromNodes(std::move(nodes), p, "CarryChain");
    carryChain_->SetAnchorMode(ChainAnchorMode::kWorld); // 手から切り離してワールド固定にし、以後はアンカーを動かして運ぶ
    carryChain_->SetAnchorPosition(anchor);
    carryChain_->SetEndWeight(carryWeight_);
    carryChain_->SetPlayerCollisionEnabled(false);

    carryTreasure_ = std::make_unique<Treasure2D>();
    carryTreasure_->Initialize(p.treasureModelDir_, p.treasureModelFile_, p.treasureScale_);
    carryTreasure_->SetDrawOffsetZ(params_.carryZ_ + kTreasureZBias);
    StepCarry(0.0f, nullptr); // 表示位置だけ合わせる
}

void TransitionDirector::BuildCarryFromData(const Vector3& anchor) {
    if (!hasCarryData_ || carryUnits_ <= 0) {
        return;
    }
    ChainParams p = carryParams_;
    p.initialUnits_ = carryUnits_;
    p.maxUnits_ = (std::max)(p.maxUnits_, carryUnits_);
    carryChain_ = std::make_unique<Chain2D>();
    carryChain_->SetDrawOffsetZ(params_.carryZ_);
    carryChain_->SetAnchorMode(ChainAnchorMode::kWorld);
    carryChain_->Initialize(anchor, p, "CarryChain"); // アンカーから真下に垂れた姿勢
    carryChain_->SetEndWeight(carryWeight_);
    carryChain_->SetPlayerCollisionEnabled(false);

    carryTreasure_ = std::make_unique<Treasure2D>();
    carryTreasure_->Initialize(p.treasureModelDir_, p.treasureModelFile_, p.treasureScale_);
    carryTreasure_->SetDrawOffsetZ(params_.carryZ_ + kTreasureZBias);
    StepCarry(0.0f, nullptr);
}

void TransitionDirector::DestroyCarryVisuals() {
    carryChain_.reset();
    carryTreasure_.reset();
}

void TransitionDirector::ClearCarryData() {
    carryUnits_ = 0;
    hasCarryData_ = false;
}

void TransitionDirector::StepCarry(float dt, MapChip2D* map) {
    if (!carryChain_) {
        return;
    }
    if (dt > 0.0f) {
        carryChain_->Update(dt, map, nullptr);
    }
    if (carryTreasure_) {
        int n = carryChain_->GetNodeCount();
        Vector3 end = carryChain_->GetEndPosition();
        Vector3 prev = (n >= 2) ? carryChain_->GetNodePosition(n - 2) : end;
        carryTreasure_->UpdateTransform(end, prev);
    }
}

Vector3 TransitionDirector::GetGemPosition() const {
    if (carryChain_) {
        return carryChain_->GetEndPosition();
    }
    if (chains_) {
        return chains_->GetTreasurePosition();
    }
    return irisCenter_;
}

// ---------------------------------------------------------------------------
// 黒板と背景板
// ---------------------------------------------------------------------------

void TransitionDirector::SetIris(const Vector3& center, float radius) {
    irisCenter_ = { center.x, center.y, 0.0f };
    irisRadius_ = (std::max)(radius, params_.irisMinScale_);
    if (iris_) {
        iris_->SetTranslation({ irisCenter_.x, irisCenter_.y, params_.irisZ_ });
        iris_->SetScale({ irisRadius_, irisRadius_, 1.0f });
    }
    if (backdrop_) {
        backdrop_->SetTranslation({ irisCenter_.x, irisCenter_.y, params_.backdropZ_ });
        backdrop_->SetScale({ params_.irisMinScale_, params_.irisMinScale_, 1.0f });
    }
}

void TransitionDirector::SetIrisToGem(float radius) {
    SetIris(GetGemPosition(), radius);
}

void TransitionDirector::SetBackdropAlpha(float alpha) {
    backdropAlpha_ = Clamp01(alpha);
    if (backdrop_) {
        backdrop_->GetMaterial().color = { 0.0f, 0.0f, 0.0f, backdropAlpha_ };
    }
}

// ---------------------------------------------------------------------------
// シネマティック（プレイヤーの演技とカメラ）
// ---------------------------------------------------------------------------

Object3D* TransitionDirector::GetActor() const {
    return player_ ? player_->GetModelObject() : nullptr;
}

Vector3 TransitionDirector::GetActorHand(Object3D* actor) const {
    if (actor) {
        if (auto joint = actor->GetJointWorldPosition(kHandJointName)) {
            return { joint->x, joint->y, 0.0f };
        }
        // ジョイントが無いモデル：胸の高さで代用
        const Vector3& t = actor->GetTranslation();
        const Vector3& s = actor->GetScale();
        return { t.x + 0.2f * s.x, t.y + 0.9f * s.y, 0.0f };
    }
    return player_ ? player_->GetPosition() : irisCenter_;
}

void TransitionDirector::HideActor() {
    // スケール0だと行列が特異になるので、極小にして画面のはるか下へ置く
    if (Object3D* actor = GetActor()) {
        Vector3 t = actor->GetTranslation();
        actor->SetTranslation({ t.x, t.y - 1000.0f, t.z });
        actor->SetScale({ 0.001f, 0.001f, 0.001f });
        actor->Update();
    }
}

void TransitionDirector::FindGoalPosition(MapChip2D* map, const Vector3& playerPos) {
    // プレイヤーに一番近いゴールチップの中心。見つからなければプレイヤーの位置
    goalPos_ = playerPos;
    if (!map) {
        return;
    }
    float bestDistSq = -1.0f;
    float half = map->GetChipSize() * 0.5f;
    for (int y = 0; y < map->GetHeight(); ++y) {
        for (int x = 0; x < map->GetWidth(); ++x) {
            if (map->GetChipType(x, y) != MapChip2D::ChipType::kGoal) {
                continue;
            }
            Vector3 c = { map->ChipToWorldX(x) + half, map->ChipToWorldY(y) + half, 0.0f };
            float dx = c.x - playerPos.x;
            float dy = c.y - playerPos.y;
            float d = dx * dx + dy * dy;
            if (bestDistSq < 0.0f || d < bestDistSq) {
                bestDistSq = d;
                goalPos_ = c;
            }
        }
    }
}

void TransitionDirector::DriveCamera(const Vector3& target, float zoom) {
    if (!camera_) {
        return;
    }
    cameraCurrent_ = { target.x, target.y, cameraStart_.z };
    currentZoom_ = (std::max)(0.2f, zoom);
    camera_->SetTranslation(cameraCurrent_);
    camera_->SetOrthoViewSize(baseOrthoW_ / currentZoom_, baseOrthoH_ / currentZoom_);
}

void TransitionDirector::RestoreCamera() {
    if (!cameraControlled_) {
        return;
    }
    cameraControlled_ = false;
    if (camera_) {
        camera_->SetOrthoViewSize(baseOrthoW_, baseOrthoH_);
        if (player_) {
            camera_->SetFollowTarget(&player_->GetPosition());
        }
    }
    currentZoom_ = 1.0f;
}

void TransitionDirector::BeginFlyBy(Object3D* actor) {
    phase_ = Phase::kFlyBy;
    phaseTime_ = 0.0f;

    // 画面の右外から左外へ
    float w = baseOrthoW_;
    flyStartX_ = cameraCurrent_.x + w * 0.5f + params_.flyMargin_;
    flyEndX_ = cameraCurrent_.x - w * 0.5f - params_.flyMargin_;

    if (actor) {
        actor->SetTranslation({ flyStartX_, cameraCurrent_.y - params_.flyScale_ * 0.5f, params_.flyZ_ });
        actor->SetRotation({ 0.0f, kPi * 0.5f, params_.flyTilt_ }); // 左を向いて傾く
        actor->SetScale({ params_.flyScale_, params_.flyScale_, params_.flyScale_ });
        actor->Update();
    }

    // 大きさを合わせた宝石と鎖（長さ・太さ・半径・重力を同じ倍率にすると同じ時間感覚で揺れる）
    if (hasCarryData_ && carryUnits_ > 0) {
        float k = params_.flyScale_;
        ChainParams p = carryParams_;
        p.initialUnits_ = carryUnits_;
        p.maxUnits_ = carryUnits_;
        p.unitLength_ *= k;
        p.nodeRadius_ *= k;
        p.linkThickness_ *= k;
        p.gravity_ *= k;
        p.treasureRadius_ *= k;
        p.treasureScale_ *= k;
        flyChain_ = std::make_unique<Chain2D>();
        flyChain_->SetDrawOffsetZ(params_.flyZ_ + kFlyChainZBias);
        flyChain_->SetAnchorMode(ChainAnchorMode::kWorld);
        flyChain_->Initialize(GetActorHand(actor), p, "FlyChain");
        EndWeight weight = carryWeight_;
        weight.radius *= k;
        flyChain_->SetEndWeight(weight);
        flyChain_->SetPlayerCollisionEnabled(false);

        flyTreasure_ = std::make_unique<Treasure2D>();
        flyTreasure_->Initialize(p.treasureModelDir_, p.treasureModelFile_, p.treasureScale_);
        flyTreasure_->SetDrawOffsetZ(params_.flyZ_ + kFlyChainZBias + kTreasureZBias);
        StepFly(0.0f, actor);
    }
}

void TransitionDirector::StepFly(float dt, Object3D* actor) {
    if (!flyChain_) {
        return;
    }
    flyChain_->SetAnchorPosition(GetActorHand(actor));
    if (dt > 0.0f) {
        flyChain_->Update(dt, nullptr, nullptr);
    }
    if (flyTreasure_) {
        int n = flyChain_->GetNodeCount();
        Vector3 end = flyChain_->GetEndPosition();
        Vector3 prev = (n >= 2) ? flyChain_->GetNodePosition(n - 2) : end;
        flyTreasure_->UpdateTransform(end, prev);
    }
}

void TransitionDirector::DestroyFly() {
    flyChain_.reset();
    flyTreasure_.reset();
}

// ---------------------------------------------------------------------------
// 開始
// ---------------------------------------------------------------------------

void TransitionDirector::StartStageClear(ChainManager* chains, MapChip2D* map, Player2D* player, GameCamera* camera) {
    if (!chains || !chains->GetPlayerChain()) {
        return;
    }
    Abort(); // 前回の残骸（中断された持ち越し等）を捨てる
    LoadParams();

    chains_ = chains;
    map_ = map;
    player_ = player;
    camera_ = camera;

    Vector3 playerPos = player_ ? player_->GetPosition() : chains->GetSocketWorld();
    cameraStart_ = camera_ ? camera_->GetTranslation() : Vector3{ playerPos.x, playerPos.y, -10.0f };
    cameraCurrent_ = cameraStart_;
    baseOrthoW_ = camera_ ? camera_->GetOrthoWidth() : 20.0f;
    baseOrthoH_ = camera_ ? camera_->GetOrthoHeight() : 11.25f;
    currentZoom_ = 1.0f;
    coverRadius_ = baseOrthoW_ + baseOrthoH_; // 穴の中心が画面端でも外側が画面を覆う
    screenBottomY_ = cameraStart_.y - baseOrthoH_ * 0.5f;

    CloneCarry(chains);
    chains->SetTransitionHidden(true);
    EnsureObjects();
    SetBackdropAlpha(0.0f);
    coveredEvent_ = false;

    Object3D* actor = GetActor();
    if (params_.clearStyle_ == 0 && actor && camera_) {
        // シネマティックの間は輪郭線をそのまま残す（切るのは黒い円が出る IrisClose から）
        // シネマティック：カメラを自分で動かす
        FindGoalPosition(map, playerPos);
        camera_->SetFollowTarget(nullptr);
        cameraControlled_ = true;
        actorBasePos_ = actor->GetTranslation();
        actorBaseYaw_ = actor->GetRotation().y;
        phase_ = Phase::kZoomIn;
        phaseTime_ = 0.0f;
        SetIris(playerPos, coverRadius_);
        Log("TransitionDirector: StageClear start (cinematic, units=" + std::to_string(carryUnits_) + ")\n");
    } else {
        // スポットライト
        phase_ = Phase::kIrisClose;
        phaseTime_ = 0.0f;
        SetIrisToGem(coverRadius_);
        Log("TransitionDirector: StageClear start (spotlight, units=" + std::to_string(carryUnits_) + ")\n");
    }
}

void TransitionDirector::StartStageOpen(ChainManager* chains, const Vector3& playerPos, float orthoWidth, float orthoHeight) {
    if (phase_ != Phase::kCovered) {
        return; // 通常の開始（持ち越し無し）は既存のフェードインに任せる
    }
    LoadParams();
    chains_ = chains;
    map_ = nullptr;
    player_ = nullptr;
    coverRadius_ = orthoWidth + orthoHeight;
    EnsureObjects();
    SetBackdropAlpha(1.0f); // 黒の中から始める

    // 着地目標はプレイヤーの手（ソケット）。最初のフレームはまだ計算されていないのでプレイヤー位置で代用する
    landFallback_ = { playerPos.x, playerPos.y + 0.5f, 0.0f };
    landDuration_ = SafeTime(params_.landTime_);

    bool canCarry = chains && chains->GetPlayerChain() && (carryChain_ || (hasCarryData_ && carryUnits_ > 0));
    if (canCarry) {
        chains->SetTransitionHidden(true);

        // 宝石が画面上端のすぐ上に来る高さ（鎖はその上に伸びている）
        float length = carryChain_ ? carryChain_->GetTotalLength()
                                   : carryParams_.unitLength_ * static_cast<float>(carryUnits_);
        landStart_ = { landFallback_.x, landFallback_.y + orthoHeight * 0.5f + length + params_.descendMargin_, 0.0f };

        if (carryChain_) {
            Vector3 delta = landStart_ - carryChain_->GetAnchorPosition();
            carryChain_->TranslateNodes(delta);
            carryChain_->SetAnchorPosition(landStart_);
            carryChain_->ResetDynamics();
        } else {
            BuildCarryFromData(landStart_); // シネマティックで巻き取った鎖を作り直す
        }
        if (carryChain_) {
            // easeOut の初速（2D/T）を鎖全体に与えて、アンカーだけが先に動いて鎖が押し潰されるのを防ぐ
            float drop = landStart_.y - landFallback_.y;
            carryChain_->SetAllVelocities({ 0.0f, -2.0f * drop / landDuration_, 0.0f }, kFixedDt);
            StepCarry(0.0f, nullptr);
        }
        phase_ = Phase::kOpenDescend;
        phaseTime_ = 0.0f;
        SetIrisToGem(params_.irisHoldRadius_);
    } else {
        DestroyCarryVisuals();
        ClearCarryData();
        phase_ = Phase::kOpenReveal;
        phaseTime_ = 0.0f;
        SetIris(landFallback_, params_.irisHoldRadius_);
    }
    Log("TransitionDirector: StageOpen start (carry=" + std::string(carryChain_ ? "yes" : "no") + ")\n");
}

void TransitionDirector::BeginDescend() {
    phase_ = Phase::kDescend;
    phaseTime_ = 0.0f;
    if (!carryChain_) {
        return;
    }
    // アンカーを「画面下端 - 鎖の全長 - 余白」まで下げれば宝石も鎖も画面の外に出る
    float length = carryChain_->GetTotalLength();
    descendStartY_ = carryChain_->GetAnchorPosition().y;
    descendDist_ = (std::max)(0.0f, descendStartY_ - screenBottomY_) + length + params_.descendMargin_;

    // easeIn(u^2) の加速度 2D/T^2 が重力を超えると鎖が追いつかずたるむので、必要なら時間を延ばす
    float g = std::fabs(carryChain_->GetParams().gravity_);
    float maxAccel = (std::max)(1.0f, g * params_.maxAnchorAccelRatio_);
    float minDuration = std::sqrt(2.0f * descendDist_ / maxAccel);
    descendDuration_ = (std::max)(SafeTime(params_.descendTime_), minDuration);
}

void TransitionDirector::BeginIrisClose() {
    // シネマティックの締め：本人はもういないので画面中央から絞る。宝石と鎖は次シーンで作り直す
    DestroyFly();
    DestroyCarryVisuals();
    HideActor(); // 飛び去った後は描かない
    phase_ = Phase::kIrisClose;
    phaseTime_ = 0.0f;
    SetIris(cameraCurrent_, coverRadius_);
}

// ---------------------------------------------------------------------------
// 更新
// ---------------------------------------------------------------------------

void TransitionDirector::Update(float dt) {
    if (phase_ == Phase::kNone || dt <= 0.0f) {
        return;
    }
    phaseTime_ += dt;

    switch (phase_) {
    // ----- シネマティック -----
    case Phase::kZoomIn: {
        // カメラがプレイヤーに寄る。本人は通常の姿勢のまま、鎖は手に付いたまま
        Object3D* actor = GetActor();
        float u = Clamp01(phaseTime_ / SafeTime(params_.zoomTime_));
        float e = EaseInOut(u);
        Vector3 focus = player_ ? player_->GetPosition() : cameraStart_;
        DriveCamera(Lerp(cameraStart_, focus, e), Lerp(1.0f, params_.zoomScale_, e));
        if (carryChain_) {
            carryChain_->SetAnchorPosition(GetActorHand(actor));
        }
        StepCarry(dt, map_);
        if (u >= 1.0f) {
            phase_ = Phase::kJoy;
            phaseTime_ = 0.0f;
            if (actor) {
                actorBasePos_ = actor->GetTranslation();
                actorBaseYaw_ = actor->GetRotation().y;
            }
        }
        break;
    }
    case Phase::kJoy: {
        // その場で跳ねて一回転（脱出を喜ぶ）
        Object3D* actor = GetActor();
        float u = Clamp01(phaseTime_ / SafeTime(params_.joyTime_));
        float hops = static_cast<float>(params_.hopCount_);
        float hop = std::fabs(std::sin(kPi * hops * u)) * params_.hopHeight_;
        float stretch = 1.0f + kHopSquash * std::sin(2.0f * kPi * hops * u); // 上昇で伸び、下降で縮む
        float yaw = actorBaseYaw_ + (params_.joySpin_ ? 2.0f * kPi * EaseInOut(u) : 0.0f);
        if (actor) {
            actor->SetTranslation({ actorBasePos_.x, actorBasePos_.y + hop, actorBasePos_.z });
            actor->SetRotation({ 0.0f, yaw, 0.0f });
            actor->SetScale({ 1.0f / stretch, stretch, 1.0f / stretch });
            actor->Update();
        }
        Vector3 focus = player_ ? player_->GetPosition() : cameraStart_;
        DriveCamera({ focus.x, focus.y + hop * 0.3f, 0.0f }, params_.zoomScale_);
        if (carryChain_) {
            carryChain_->SetAnchorPosition(GetActorHand(actor));
        }
        StepCarry(dt, map_);
        if (u >= 1.0f) {
            phase_ = Phase::kEnterGoal;
            phaseTime_ = 0.0f;
            enterStart_ = actorBasePos_;
        }
        break;
    }
    case Phase::kEnterGoal: {
        // ゴールへ歩き、背を向けて奥へ小さくなって消える。鎖は手元へ巻き取られる
        Object3D* actor = GetActor();
        float u = Clamp01(phaseTime_ / SafeTime(params_.enterGoalTime_));
        float e = EaseInOut(u);
        float turn = Clamp01(u / 0.4f); // 最初の4割で向きを変える
        float yaw = Lerp(actorBaseYaw_, params_.enterGoalYaw_, EaseInOut(turn));
        float shrink = 1.0f - EaseIn(u);
        float depth = params_.enterGoalDepth_ * EaseIn(u);
        if (actor) {
            actor->SetTranslation({ Lerp(enterStart_.x, goalPos_.x, e), enterStart_.y, enterStart_.z + depth });
            actor->SetRotation({ 0.0f, yaw, 0.0f });
            actor->SetScale({ shrink, shrink, shrink });
            actor->Update();
        }
        Vector3 focus = player_ ? player_->GetPosition() : cameraStart_;
        DriveCamera(Lerp(focus, goalPos_, e * 0.5f), params_.zoomScale_);
        if (carryChain_) {
            carryChain_->SetAnchorPosition(GetActorHand(actor));
            // 手元へ巻き取る（残りユニット数を進行に合わせて減らす）
            int desired = (std::max)(1, static_cast<int>(std::ceil(static_cast<float>(carryUnits_) * (1.0f - u))));
            int current = carryChain_->GetUnitCount();
            if (current > desired) {
                carryChain_->RemoveUnitsAtAnchor(current - desired);
            }
        }
        StepCarry(dt, map_);
        if (u >= 1.0f) {
            DestroyCarryVisuals(); // 本人と一緒に消える（個数は控えてある）
            BeginFlyBy(actor);
        }
        break;
    }
    case Phase::kFlyBy: {
        // 大きくなってカメラの前を右から左へ飛び抜ける
        Object3D* actor = GetActor();
        float u = Clamp01(phaseTime_ / SafeTime(params_.flyTime_));
        // 寄っていたカメラは最初の2割で元の広さに戻す
        float zoomBack = EaseOut(Clamp01(u / 0.2f));
        DriveCamera(cameraCurrent_, Lerp(params_.zoomScale_, 1.0f, zoomBack));
        if (actor) {
            float x = Lerp(flyStartX_, flyEndX_, u);
            float y = cameraCurrent_.y - params_.flyScale_ * 0.5f + params_.flyArcHeight_ * std::sin(kPi * u);
            actor->SetTranslation({ x, y, params_.flyZ_ });
            actor->SetRotation({ 0.0f, kPi * 0.5f, params_.flyTilt_ });
            actor->SetScale({ params_.flyScale_, params_.flyScale_, params_.flyScale_ });
            actor->Update();
        }
        StepFly(dt, actor);
        if (u >= 1.0f) {
            BeginIrisClose();
        }
        break;
    }

    // ----- 円で覆う -----
    case Phase::kIrisClose: {
        float u = Clamp01(phaseTime_ / SafeTime(params_.irisCloseTime_));
        float e = EaseInOut(u);
        SetBackdropAlpha(e);
        if (params_.clearStyle_ == 0) {
            // シネマティック：画面中央から閉じ切って暗転。本人は飛び去ったので毎フレーム隠す
            HideActor();
            SetIris(cameraCurrent_, Lerp(coverRadius_, params_.irisMinScale_, e));
            if (u >= 1.0f) {
                RestoreCamera();
                phase_ = Phase::kCovered;
                phaseTime_ = 0.0f;
                coveredEvent_ = true;
                chains_ = nullptr;
                map_ = nullptr;
                player_ = nullptr;
            }
        } else {
            // スポットライト：穴を宝石へ絞りながら背景板をフェードイン。鎖の物理は動いたまま（床にも当たる）
            StepCarry(dt, map_);
            SetIrisToGem(Lerp(coverRadius_, params_.irisHoldRadius_, e));
            if (u >= 1.0f) {
                phase_ = Phase::kHold;
                phaseTime_ = 0.0f;
            }
        }
        break;
    }
    case Phase::kHold:
        StepCarry(dt, map_);
        SetIrisToGem(params_.irisHoldRadius_);
        if (phaseTime_ >= params_.holdTime_) {
            BeginDescend();
        }
        break;

    case Phase::kDescend: {
        float u = Clamp01(phaseTime_ / descendDuration_);
        if (carryChain_) {
            Vector3 anchor = carryChain_->GetAnchorPosition();
            anchor.y = descendStartY_ - descendDist_ * EaseIn(u);
            carryChain_->SetAnchorPosition(anchor);
        }
        StepCarry(dt, nullptr); // 黒の上を降りる（ステージは背景板の奥なので貫通は見えない）
        SetIrisToGem(params_.irisHoldRadius_);
        if (u >= 1.0f) {
            phase_ = Phase::kIrisFinish;
            phaseTime_ = 0.0f;
        }
        break;
    }
    case Phase::kIrisFinish: {
        float u = Clamp01(phaseTime_ / SafeTime(params_.irisFinishTime_));
        StepCarry(dt, nullptr);
        SetIrisToGem(Lerp(params_.irisHoldRadius_, params_.irisMinScale_, EaseIn(u)));
        if (u >= 1.0f) {
            phase_ = Phase::kCovered;
            phaseTime_ = 0.0f;
            coveredEvent_ = true;
            // シーンが切り替わるので現在のシーンの持ち物への参照を切る（持ち越す鎖は自前）
            chains_ = nullptr;
            map_ = nullptr;
            player_ = nullptr;
            SetIris(irisCenter_, params_.irisMinScale_);
        }
        break;
    }
    case Phase::kCovered:
        break; // シーン側の ChangeScene 待ち

    // ----- 次シーン -----
    case Phase::kOpenDescend: {
        // 黒の中を、穴と一緒に宝石と鎖が上から手元へ降りる
        Vector3 target = landFallback_;
        if (chains_) {
            Vector3 socket = chains_->GetSocketWorld();
            if (std::fabs(socket.x) > 1e-4f || std::fabs(socket.y) > 1e-4f) { // まだ計算前（原点のまま）なら代用位置
                target = { socket.x, socket.y, 0.0f };
            }
        }
        float ul = Clamp01(phaseTime_ / landDuration_);
        if (carryChain_) {
            Vector3 anchor = landStart_ + (target - landStart_) * EaseOut(ul);
            carryChain_->SetAnchorPosition(anchor);
            StepCarry(dt, nullptr);
        }
        SetIrisToGem(params_.irisHoldRadius_);
        if (ul >= 1.0f || !carryChain_) {
            // 着地：ゲーム側の鎖に引き継ぐ（同じ個数で生成されているので手元に垂れた姿勢がそのまま続く）
            if (chains_) {
                chains_->SetTransitionHidden(false);
            }
            DestroyCarryVisuals();
            ClearCarryData();
            phase_ = Phase::kOpenReveal;
            phaseTime_ = 0.0f;
        }
        break;
    }
    case Phase::kOpenReveal: {
        // 宝石を中心に穴が開き、背景板が消えてステージが現れる
        float u = Clamp01(phaseTime_ / SafeTime(params_.irisOpenTime_));
        float e = EaseOut(u);
        SetBackdropAlpha(1.0f - e);
        SetIrisToGem(Lerp(params_.irisHoldRadius_, coverRadius_, e));
        if (u >= 1.0f) {
            Finish();
        }
        break;
    }
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// 終了・描画
// ---------------------------------------------------------------------------

void TransitionDirector::Finish() {
    RestoreCamera();
    if (chains_) {
        chains_->SetTransitionHidden(false);
    }
    DestroyFly();
    DestroyCarryVisuals();
    ClearCarryData();
    SetBackdropAlpha(0.0f);
    chains_ = nullptr;
    map_ = nullptr;
    player_ = nullptr;
    phase_ = Phase::kNone;
    phaseTime_ = 0.0f;
    coveredEvent_ = false;
    Log("TransitionDirector: finished\n");
}

void TransitionDirector::Abort() {
    Finish();
}

void TransitionDirector::OnSceneDestroyed(ChainManager* chains) {
    if (chains_ == chains) {
        chains_ = nullptr;
        map_ = nullptr;
        player_ = nullptr; // プレイヤーはシーンと一緒に消える
    }
    if (phase_ != Phase::kCovered) {
        // 覆い切る前に消えた／開き切る前に消えた → 持ち越しごと捨てる
        Finish();
    }
}

void TransitionDirector::Draw() {
    if (phase_ == Phase::kNone) {
        return;
    }
    // 背景板（半透明はステージの後に描く）→ 宝石と鎖 → 黒板。前後は深度で決まる
    if (backdrop_ && backdropAlpha_ > 0.0f) {
        backdrop_->Update();
        backdrop_->Draw();
    }
    if (carryChain_) {
        carryChain_->Draw();
    }
    if (carryTreasure_) {
        carryTreasure_->Draw();
    }
    if (flyChain_) {
        flyChain_->Draw();
    }
    if (flyTreasure_) {
        flyTreasure_->Draw();
    }
    if (iris_ && irisRadius_ < coverRadius_) {
        iris_->Update();
        iris_->Draw();
    }
}

bool TransitionDirector::ConsumeCoveredEvent() {
    bool fired = coveredEvent_;
    coveredEvent_ = false;
    return fired;
}

bool TransitionDirector::IsCovering() const {
    return phase_ == Phase::kIrisClose || phase_ == Phase::kHold ||
           phase_ == Phase::kDescend || phase_ == Phase::kIrisFinish || phase_ == Phase::kCovered;
}

std::vector<Object3D*> TransitionDirector::GetObjects() const {
    std::vector<Object3D*> result;
    if (phase_ == Phase::kNone) {
        return result;
    }
    if (carryChain_) {
        auto links = carryChain_->GetLinkObjects();
        result.insert(result.end(), links.begin(), links.end());
    }
    if (carryTreasure_ && carryTreasure_->GetObject()) {
        result.push_back(carryTreasure_->GetObject());
    }
    if (flyChain_) {
        auto links = flyChain_->GetLinkObjects();
        result.insert(result.end(), links.begin(), links.end());
    }
    if (flyTreasure_ && flyTreasure_->GetObject()) {
        result.push_back(flyTreasure_->GetObject());
    }
    if (backdrop_) {
        result.push_back(backdrop_.get());
    }
    if (iris_) {
        result.push_back(iris_.get());
    }
    return result;
}

void TransitionDirector::DrawImGui() {
#ifdef USE_IMGUI
    const char* names[] = { "None", "ZoomIn", "Joy", "EnterGoal", "FlyBy", "IrisClose", "Hold", "Descend", "IrisFinish", "Covered", "OpenDescend", "OpenReveal" };
    ImGui::Text("Transition: %s  t=%.2f  iris r=%.2f  backdrop a=%.2f  zoom %.2f  carry=%s(%d units)",
                names[static_cast<int>(phase_)], phaseTime_, irisRadius_, backdropAlpha_, currentZoom_,
                (carryChain_ || hasCarryData_) ? "yes" : "no", carryUnits_);
    ImGui::TextDisabled("パラメータは ParameterManager の \"Transition\" グループ（clearStyle 0=シネマティック / 1=スポットライト。次回の遷移開始時に反映）");
#endif
}
