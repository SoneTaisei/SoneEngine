#include "Sprite.h"
#include "Renderer/Renderer.h"
#include "SpriteCommon.h"
#include "Graphics/TextureManager.h"

Sprite::Sprite() {}

Sprite::~Sprite() {
	// 遐ｴ譽・＆繧後ｋ縺ｨ縺阪↓繝ｪ繧ｹ繝医°繧芽・蛻・ｒ蜑企勁
	if(spriteCommon_) {
		spriteCommon_->RemoveSprite(this);
	}
}

void Sprite::Initialize(SpriteCommon *spriteCommon, uint32_t textureIndex) {
	spriteCommon_ = spriteCommon;
	textureIndex_ = textureIndex;

	// 笘・％縺薙〒Common縺ｫ閾ｪ蛻・ｒ逋ｻ骭ｲ・・
	spriteCommon_->AddSprite(this);

	// 繝槭ユ繝ｪ繧｢繝ｫ繝ｪ繧ｽ繝ｼ繧ｹ菴懈・
	ID3D12Device *device = spriteCommon_->GetDevice();
	materialResource_ = CreateBufferResource(device, sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&materialData_));

	size_t sizeAligned = (sizeof(TransformMatrix) + 0xff) & ~0xff;

	// 陦悟・逕ｨ縺ｮ繝舌ャ繝輔ぃ繝ｪ繧ｽ繝ｼ繧ｹ繧剃ｽ懈・
    transformResource_ = CreateBufferResource(device,sizeAligned);
    // 譖ｸ縺崎ｾｼ縺ｿ逕ｨ縺ｮ繝昴う繝ｳ繧ｿ繧堤ｴ蝉ｻ倥￠繧・
    transformResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));

	// 蛻晄悄蛟､險ｭ螳・
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->lightingType = false;
	materialData_->uvTransform = TransformFunctions::MakeIdentity4x4();

	// 繝・け繧ｹ繝√Ε繧ｵ繧､繧ｺ繧貞叙蠕暦ｼ亥・繧頑栢縺崎ｨ育ｮ礼畑・・
	const D3D12_RESOURCE_DESC resDesc = TextureManager::GetInstance()->GetResourceDesc(textureIndex);
	texBaseSize_ = { (float)resDesc.Width, (float)resDesc.Height };
	texSize_ = texBaseSize_; // 繝・ヵ繧ｩ繝ｫ繝医・蜈ｨ遽・峇

	if (mappedTransform_) {
        mappedTransform_->WVP = TransformFunctions::MakeIdentity4x4();
        mappedTransform_->World = TransformFunctions::MakeIdentity4x4();
    }
}

void Sprite::Update() {
	// 蠢・ｦ√↑繧峨％縺薙〒UV繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ蜃ｦ逅・↑縺ｩ
}

void Sprite::SetTextureRect(float x, float y, float w, float h) {
	texPos_ = { x, y };
	texSize_ = { w, h };
	isCutMode_ = true;
}

void Sprite::Draw() {
    Renderer::GetInstance()->DrawSprite(this);
}
