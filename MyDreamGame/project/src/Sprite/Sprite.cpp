#include "Sprite.h"
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

	// ★ここでCommonに自分を登録！
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
	if(!spriteCommon_) return;

	// UVトランスフォームの計算 (切り抜き対応)
	Matrix4x4 uvTransform = TransformFunctions::MakeIdentity4x4();
	if(isCutMode_) {
		uvTransform = TransformFunctions::MakeAffineMatrix(
			{ texSize_.x / texBaseSize_.x, texSize_.y / texBaseSize_.y, 1.0f },
			{ 0.0f, 0.0f, 0.0f },
			{ texPos_.x / texBaseSize_.x, texPos_.y / texBaseSize_.y, 0.0f }
		);
	}
	materialData_->uvTransform = uvTransform;

	// WorldViewProjection行列の計算
	Matrix4x4 worldMatrix = TransformFunctions::MakeAffineMatrix(
		transform_.scale, transform_.rotate, transform_.translate
	);
    Matrix4x4 viewMatrix = TransformFunctions::MakeIdentity4x4();
    Matrix4x4 projectionMatrix = spriteCommon_->GetProjectionMatrix();

    // TransformMatrix構造体をGPUに送る
    TransformMatrix transformMatrixData;
    transformMatrixData.WVP = TransformFunctions::Multiply(worldMatrix, TransformFunctions::Multiply(viewMatrix, projectionMatrix));
    transformMatrixData.World = worldMatrix;

	// 計算結果をGPU上のメモリにコピー！
    if (mappedTransform_) {
        *mappedTransform_ = transformMatrixData;
    }

	// コマンドリストへの設定
	ID3D12GraphicsCommandList *commandList = spriteCommon_->GetCommandList();

	// マテリアル (RootParameter 0番)
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// 行列データ (RootParameter 1番: 32BitConstantsを使用していた場合の例)
	// ※元のSprite.cppに合わせ、直接値をセットしています
	//commandList->SetGraphicsRoot32BitConstants(1, sizeof(TransformMatrix) / 4, &transformMatrixData, 0);
    commandList->SetGraphicsRootConstantBufferView(1, transformResource_->GetGPUVirtualAddress());

	// テクスチャ (RootParameter 2番)
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = TextureManager::GetInstance()->GetGpuHandle(textureIndex_);
	commandList->SetGraphicsRootDescriptorTable(2, textureHandle);

	// 描画実行 (頂点・インデックスはCommonでセット済み)
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}