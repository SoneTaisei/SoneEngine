#include "Sprite.h"
#include "Renderer/Renderer.h"
#include "SpriteCommon.h"
#include "Graphics/TextureManager.h"
#include "Core/TimeManager.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

Sprite::Sprite() {}

Sprite::~Sprite() {
	// 破棄されるときにリストから自分を削除
	if(spriteCommon_) {
		spriteCommon_->RemoveSprite(this);
	}
}

void Sprite::Initialize(SpriteCommon *spriteCommon, uint32_t textureIndex) {
	spriteCommon_ = spriteCommon;
	textureIndex_ = textureIndex;

	// ここでCommonに自分を登録！
	spriteCommon_->AddSprite(this);

	// マテリアルリソース作成
	ID3D12Device *device = spriteCommon_->GetDevice();
	materialResource_ = CreateBufferResource(device, sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&materialData_));

	size_t sizeAligned = (sizeof(TransformMatrix) + 0xff) & ~0xff;

	// 行列用のバッファリソースを作成
    transformResource_ = CreateBufferResource(device,sizeAligned);
    // 書き込み用のポインタを紐付ける
    transformResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));

	// 初期値設定
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->lightingType = false;
	materialData_->uvTransform = TransformFunctions::MakeIdentity4x4();

	// テクスチャサイズを取得（切り抜き計算用）
	const D3D12_RESOURCE_DESC resDesc = TextureManager::GetInstance()->GetResourceDesc(textureIndex);
	texBaseSize_ = { (float)resDesc.Width, (float)resDesc.Height };
	texSize_ = texBaseSize_; // デフォルトは全範囲

	if (mappedTransform_) {
        mappedTransform_->WVP = TransformFunctions::MakeIdentity4x4();
        mappedTransform_->World = TransformFunctions::MakeIdentity4x4();
    }
}

void Sprite::Update() {
    Update(TimeManager::GetInstance().GetDeltaTime());
}

void Sprite::Update(float deltaTime) {
    if (!isAnimationActive_ || !isAnimPlaying_ || animTotalFrames_ <= 1) {
        return;
    }

    float frameDuration = (animConfig_.fps > 0.0f) ? (1.0f / animConfig_.fps) : 0.1f;
    animTimer_ += deltaTime;

    while (animTimer_ >= frameDuration) {
        animTimer_ -= frameDuration;
        int nextFrame = animCurrentFrame_ + 1;
        if (nextFrame >= animTotalFrames_) {
            if (animConfig_.isLoop) {
                nextFrame = 0;
            } else {
                nextFrame = animTotalFrames_ - 1;
                isAnimPlaying_ = false;
                SetAnimationFrame(nextFrame);
                break;
            }
        }
        SetAnimationFrame(nextFrame);
    }
}

void Sprite::SetAnimationGrid(int columns, int rows, float fps, bool isLoop, int totalFrames) {
    SpriteAnimationConfig config;
    config.columns = (columns > 0) ? columns : 1;
    config.rows = (rows > 0) ? rows : 1;
    config.totalFrames = totalFrames;
    config.fps = (fps > 0.0f) ? fps : 10.0f;
    config.isLoop = isLoop;
    SetAnimation(config);
}

void Sprite::SetAnimation(const SpriteAnimationConfig &config) {
    animConfig_ = config;
    if (animConfig_.columns < 1) animConfig_.columns = 1;
    if (animConfig_.rows < 1) animConfig_.rows = 1;
    if (animConfig_.fps <= 0.0f) animConfig_.fps = 10.0f;

    animTotalFrames_ = (animConfig_.totalFrames > 0) ? animConfig_.totalFrames : (animConfig_.columns * animConfig_.rows);
    if (animTotalFrames_ < 1) animTotalFrames_ = 1;

    isAnimationActive_ = true;
    isAnimPlaying_ = true;
    animTimer_ = 0.0f;
    animCurrentFrame_ = 0;

    SetAnimationFrame(0);
}

void Sprite::PlayAnimation() {
    isAnimationActive_ = true;
    isAnimPlaying_ = true;
}

void Sprite::PauseAnimation() {
    isAnimPlaying_ = false;
}

void Sprite::ResumeAnimation() {
    if (isAnimationActive_) {
        isAnimPlaying_ = true;
    }
}

void Sprite::StopAnimation() {
    isAnimPlaying_ = false;
    animTimer_ = 0.0f;
    SetAnimationFrame(0);
}

void Sprite::SetAnimationFrame(int frameIndex) {
    if (!isAnimationActive_ || animTotalFrames_ <= 0) return;

    if (frameIndex < 0) frameIndex = 0;
    if (frameIndex >= animTotalFrames_) frameIndex = animTotalFrames_ - 1;

    animCurrentFrame_ = frameIndex;

    float frameWidth = texBaseSize_.x / static_cast<float>(animConfig_.columns);
    float frameHeight = texBaseSize_.y / static_cast<float>(animConfig_.rows);

    int col = animCurrentFrame_ % animConfig_.columns;
    int row = animCurrentFrame_ / animConfig_.columns;

    float x = static_cast<float>(col) * frameWidth;
    float y = static_cast<float>(row) * frameHeight;

    SetTextureRect(x, y, frameWidth, frameHeight);
}

void Sprite::SetTextureRect(float x, float y, float w, float h) {
	texPos_ = { x, y };
	texSize_ = { w, h };
	isCutMode_ = true;
}

void Sprite::ResetTextureRect() {
    texPos_ = { 0.0f, 0.0f };
    texSize_ = texBaseSize_;
    isCutMode_ = false;
}

void Sprite::Draw() {
    Renderer::GetInstance()->DrawSprite(this);
}

bool Sprite::InitializeFromConfig(SpriteCommon *spriteCommon, const std::string &jsonPath) {
    if (!std::filesystem::exists(jsonPath)) {
        return false;
    }
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        return false;
    }
    nlohmann::json j;
    try {
        file >> j;
    } catch (...) {
        return false;
    }

    std::string texPath = j.value("texturePath", "");
    if (texPath.empty()) {
        return false;
    }

    uint32_t texHandle = TextureManager::GetInstance()->Load(texPath);
    Initialize(spriteCommon, texHandle);

    animConfig_.texturePath = texPath;
    animConfig_.columns = j.value("columns", 1);
    animConfig_.rows = j.value("rows", 1);
    animConfig_.totalFrames = j.value("totalFrames", 0);
    animConfig_.fps = j.value("fps", 10.0f);
    animConfig_.isLoop = j.value("isLoop", true);

    SetAnimation(animConfig_);

    if (j.contains("size") && j["size"].is_array() && j["size"].size() >= 2) {
        float sw = j["size"][0].get<float>();
        float sh = j["size"][1].get<float>();
        if (sw > 0.0f && sh > 0.0f) {
            SetSize({ sw, sh });
        }
    } else {
        float frameW = (animConfig_.columns > 0) ? (texBaseSize_.x / static_cast<float>(animConfig_.columns)) : texBaseSize_.x;
        float frameH = (animConfig_.rows > 0) ? (texBaseSize_.y / static_cast<float>(animConfig_.rows)) : texBaseSize_.y;
        SetSize({ frameW, frameH });
    }

    if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 2) {
        SetPosition({ j["position"][0].get<float>(), j["position"][1].get<float>() });
    }

    if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 4) {
        SetColor({ j["color"][0].get<float>(), j["color"][1].get<float>(), j["color"][2].get<float>(), j["color"][3].get<float>() });
    }

    PlayAnimation();
    return true;
}

bool Sprite::LoadAnimationConfig(const std::string &jsonPath) {
    if (!std::filesystem::exists(jsonPath)) {
        return false;
    }
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        return false;
    }
    nlohmann::json j;
    try {
        file >> j;
    } catch (...) {
        return false;
    }

    std::string texPath = j.value("texturePath", "");
    if (!texPath.empty()) {
        textureIndex_ = TextureManager::GetInstance()->Load(texPath);
        const D3D12_RESOURCE_DESC resDesc = TextureManager::GetInstance()->GetResourceDesc(textureIndex_);
        texBaseSize_ = { static_cast<float>(resDesc.Width), static_cast<float>(resDesc.Height) };
    }

    animConfig_.texturePath = texPath;
    animConfig_.columns = j.value("columns", 1);
    animConfig_.rows = j.value("rows", 1);
    animConfig_.totalFrames = j.value("totalFrames", 0);
    animConfig_.fps = j.value("fps", 10.0f);
    animConfig_.isLoop = j.value("isLoop", true);

    SetAnimation(animConfig_);

    if (j.contains("size") && j["size"].is_array() && j["size"].size() >= 2) {
        float sw = j["size"][0].get<float>();
        float sh = j["size"][1].get<float>();
        if (sw > 0.0f && sh > 0.0f) {
            SetSize({ sw, sh });
        }
    } else {
        float frameW = (animConfig_.columns > 0) ? (texBaseSize_.x / static_cast<float>(animConfig_.columns)) : texBaseSize_.x;
        float frameH = (animConfig_.rows > 0) ? (texBaseSize_.y / static_cast<float>(animConfig_.rows)) : texBaseSize_.y;
        SetSize({ frameW, frameH });
    }

    if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 2) {
        SetPosition({ j["position"][0].get<float>(), j["position"][1].get<float>() });
    }

    if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 4) {
        SetColor({ j["color"][0].get<float>(), j["color"][1].get<float>(), j["color"][2].get<float>(), j["color"][3].get<float>() });
    }

    PlayAnimation();
    return true;
}

bool Sprite::SaveAnimationConfig(const std::string &jsonPath) const {
    nlohmann::json j;
    j["texturePath"] = animConfig_.texturePath;
    j["columns"] = animConfig_.columns;
    j["rows"] = animConfig_.rows;
    j["totalFrames"] = animTotalFrames_;
    j["fps"] = animConfig_.fps;
    j["isLoop"] = animConfig_.isLoop;
    Vector2 sz = GetSize();
    j["size"] = { sz.x, sz.y };
    Vector2 pos = GetPosition();
    j["position"] = { pos.x, pos.y };
    if (materialData_) {
        j["color"] = { materialData_->color.x, materialData_->color.y, materialData_->color.z, materialData_->color.w };
    } else {
        j["color"] = { 1.0f, 1.0f, 1.0f, 1.0f };
    }

    std::filesystem::path p(jsonPath);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    std::ofstream file(jsonPath);
    if (!file.is_open()) {
        return false;
    }
    file << j.dump(4);
    return true;
}


