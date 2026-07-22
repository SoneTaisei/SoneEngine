#include "Sprite.h"
#include "Renderer/Renderer.h"
#include "SpriteCommon.h"
#include "Graphics/TextureManager.h"

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
	// 必要ならここでUVアニメーション処理など
}

void Sprite::SetTextureRect(float x, float y, float w, float h) {
	texPos_ = { x, y };
	texSize_ = { w, h };
	isCutMode_ = true;
}

void Sprite::Draw() {
    Renderer::GetInstance()->DrawSprite(this);
}
