#include "TutorialPoster.h"
#include "GameObject/PrimitiveObject.h"
#include "Resource/Primitive/PrimitiveManager.h"
#include "Graphics/TextureManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Core/Utility/UtilityFunctions.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>

namespace {
    // 現れる・消える速さ（1 秒あたりのアルファの変化）
    constexpr float kFadeSpeed = 4.0f;
    // ブロック（z = 0、厚み 1）の奥、背景板（z = 1.6）の手前
    constexpr float kPosterZ = 1.2f;
    // 枠は映像のさらに少し奥（同じ z だと重なってちらつく）
    constexpr float kFrameZ = 1.25f;
}

TutorialPoster::TutorialPoster() = default;
TutorialPoster::~TutorialPoster() = default;

void TutorialPoster::Initialize(ID3D12Device* device, const std::string& metaPath) {
    obj_.reset();
    frameObj_.reset();
    if (!device) return;

    // ---- メタ JSON ----
    std::ifstream ifs(metaPath);
    if (!ifs) {
        Log("TutorialPoster: meta not found: " + metaPath + "\n");
        return;
    }
    try {
        nlohmann::json j;
        ifs >> j;
        meta_.texture = j.value("texture", std::string());
        meta_.cellW = j.value("cellW", 320);
        meta_.cellH = j.value("cellH", 180);
        meta_.cols = (std::max)(1, j.value("cols", 8));
        meta_.frames = (std::max)(1, j.value("frames", 1));
        meta_.fps = (std::max)(1.0f, j.value("fps", 12.0f));
        meta_.loop = j.value("loop", true);
        meta_.holdLastFrames = (std::max)(0, j.value("holdLastFrames", 0));
        meta_.sequence.clear();
        if (j.contains("sequence") && j["sequence"].is_array()) {
            for (const auto& v : j["sequence"]) {
                if (v.is_number_integer()) meta_.sequence.push_back(std::clamp(v.get<int>(), 0, meta_.frames - 1));
            }
        }
    } catch (...) {
        Log("TutorialPoster: meta parse error: " + metaPath + "\n");
        return;
    }
    if (meta_.texture.empty() || !std::filesystem::exists(meta_.texture)) {
        Log("TutorialPoster: texture not found: " + meta_.texture + "\n");
        return;
    }

    // ---- 板（Plane は XZ 面なので X 軸で -90 度回して正面（-Z）を向かせる。背景板と同じ） ----
    Primitive* plane = PrimitiveManager::GetInstance()->GetPrimitive(PrimitiveType::Plane, 1.0f);
    if (!plane) return;
    obj_ = std::make_unique<PrimitiveObject>();
    obj_->Initialize(device, plane);
    obj_->SetName("TutorialPoster");
    obj_->SetRotation({ -std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f });
    obj_->SetIsBillboard(false);
    obj_->SetIsDoubleSided(true);
    // テクスチャはここでは読まない（EnsureTexture で、近づいた時に初めて読む）
    Material& mat = obj_->GetMaterial();
    mat.lightingType = 0; // 明かりに関係なく読めるように
    mat.enableEnvironmentMap = 0;
    mat.enableBoxMapping = 0.0f;
    mat.color = { 1.0f, 1.0f, 1.0f, 0.0f };

    // ---- 枠（映像の外側に一回り大きい単色の板を置く。ステージの背景と見分けるため） ----
    if (frameMargin_ > 0.0f) {
        frameObj_ = std::make_unique<PrimitiveObject>();
        frameObj_->Initialize(device, plane);
        frameObj_->SetName("TutorialPosterFrame");
        frameObj_->SetRotation({ -std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f });
        frameObj_->SetIsBillboard(false);
        frameObj_->SetIsDoubleSided(true);
        Material& fmat = frameObj_->GetMaterial();
        fmat.lightingType = 0;
        fmat.enableEnvironmentMap = 0;
        fmat.enableBoxMapping = 0.0f;
        fmat.color = { frameColor_.x, frameColor_.y, frameColor_.z, 0.0f };
    }

    SetPlacement(center_, width_, height_);
    ApplyFrame(0);
    obj_->Update();
    if (frameObj_) frameObj_->Update();
}

void TutorialPoster::SetPlacement(const Vector3& center, float width, float height) {
    center_ = center;
    width_ = (std::max)(0.1f, width);
    height_ = (std::max)(0.1f, height);
    if (!obj_) return;
    // Plane は XZ 面：X が横、Z が縦（回転後は Y）
    obj_->SetScale({ width_, 1.0f, height_ });
    obj_->SetTranslation({ center_.x, center_.y, kPosterZ });
    if (frameObj_) {
        frameObj_->SetScale({ width_ + frameMargin_ * 2.0f, 1.0f, height_ + frameMargin_ * 2.0f });
        frameObj_->SetTranslation({ center_.x, center_.y, kFrameZ });
    }
}

void TutorialPoster::EnsureTexture() {
    if (textureLoaded_ || !obj_ || meta_.texture.empty()) return;
    uint32_t tex = TextureManager::GetInstance()->Load(meta_.texture);
    obj_->SetTextureHandle(TextureManager::GetInstance()->GetGpuHandle(tex));
    textureLoaded_ = true;
}

void TutorialPoster::ApplyFrame(int frame) {
    if (!obj_) return;
    frame = std::clamp(frame, 0, meta_.frames - 1);
    if (frame == frame_) return;
    frame_ = frame;
    // シート全体の UV（0〜1）の中で、このコマの位置と大きさ
    const int rows = (meta_.frames + meta_.cols - 1) / meta_.cols;
    const float su = 1.0f / static_cast<float>(meta_.cols);
    const float sv = 1.0f / static_cast<float>(rows);
    const float tu = static_cast<float>(frame % meta_.cols) * su;
    const float tv = static_cast<float>(frame / meta_.cols) * sv;
    obj_->GetMaterial().uvTransform = TransformFunctions::MakeAffineMatrix({ su, sv, 1.0f }, { 0.0f, 0.0f, 0.0f }, { tu, tv, 0.0f });
}

void TutorialPoster::Update(float dt, const Vector3& playerPos, bool active) {
    if (!obj_) return;

    // ---- 対象（板の範囲）からの距離で出す・消すを決める（ヒステリシス付き） ----
    const float dx = (std::max)({ tMinX_ - playerPos.x, 0.0f, playerPos.x - tMaxX_ });
    const float dy = (std::max)({ tMinY_ - playerPos.y, 0.0f, playerPos.y - tMaxY_ });
    const float dist = std::sqrt(dx * dx + dy * dy);

    // 出す少し前に読み込む。歩いて近づく間に済ませておけば、出た瞬間に固まらない
    constexpr float kPreloadMargin = 14.0f;
    if (!textureLoaded_ && (preview_ || alwaysShow_ || dist <= hideDist_ + kPreloadMargin)) {
        EnsureTexture();
    }

    if (preview_) {
        shown_ = true; // 編集中：いつでも見せる
    } else if (!active) {
        shown_ = false;
    } else if (alwaysShow_) {
        shown_ = true;
    } else if (!shown_ && dist <= showDist_) {
        shown_ = true;
        time_ = 0.0f; // 近づいた時は最初のコマから
    } else if (shown_ && dist >= hideDist_) {
        shown_ = false;
    }
    const float targetAlpha = shown_ ? 1.0f : 0.0f;
    if (alpha_ < targetAlpha) alpha_ = (std::min)(targetAlpha, alpha_ + kFadeSpeed * dt);
    else if (alpha_ > targetAlpha) alpha_ = (std::max)(targetAlpha, alpha_ - kFadeSpeed * dt);

    // ---- コマ送り（見えている間だけ進める） ----
    if (alpha_ > 0.001f) {
        time_ += dt;
        // 再生順が指定されていればその長さ、無ければコマ数がひと回り
        const int steps = meta_.sequence.empty() ? meta_.frames : static_cast<int>(meta_.sequence.size());
        const float cycleFrames = static_cast<float>(steps + meta_.holdLastFrames);
        float f = time_ * meta_.fps;
        if (meta_.loop) {
            f = std::fmod(f, cycleFrames);
        } else {
            f = (std::min)(f, static_cast<float>(steps - 1));
        }
        const int step = (std::min)(static_cast<int>(f), steps - 1); // 最後の holdLastFrames 分は最後のコマに留まる
        ApplyFrame(meta_.sequence.empty() ? step : meta_.sequence[step]);
    }
    obj_->GetMaterial().color = { 1.0f, 1.0f, 1.0f, alpha_ };
    obj_->Update();
    if (frameObj_) {
        frameObj_->GetMaterial().color = { frameColor_.x, frameColor_.y, frameColor_.z, alpha_ };
        frameObj_->Update();
    }
}

void TutorialPoster::Draw() {
    if (!obj_ || !textureLoaded_ || alpha_ <= 0.001f) return;
    if (frameObj_) frameObj_->Draw(); // 先に枠、その手前に映像
    obj_->Draw();
}
