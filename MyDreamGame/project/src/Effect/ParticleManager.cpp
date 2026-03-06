#include "ParticleManager.h"
#include "Graphics/TextureManager.h"
#include "Graphics/SrvManager.h"
#include "Utility/TransformFunctions.h"
#include <cassert>
#include <random>
//#include "imgui.h"

ParticleManager::~ParticleManager() {
    if(particleCommon_) {
        particleCommon_->RemoveParticle(this);
    }
}

void ParticleManager::Initialize(ID3D12GraphicsCommandList *commandList,ParticleCommon *particleCommon, uint32_t count, const std::string &textureFilePath, int srvIndex, BlendMode blendMode) {
    particleCommon_ = particleCommon;
    kParticleCount_ = count;
    ID3D12Device *device = particleCommon_->GetDevice();
    blendMode_ = blendMode;

    // --- 乱数生成器の準備 ---
    std::random_device seedGenerator;
    randomEngine_.seed(seedGenerator());

    // 初期化時はリストをクリアするだけ（最初はパーティクル0個）
    particles_.clear();

    // エミッタのデフォルト設定
    emitter_.count = 3;           // 1回で3個出る
    emitter_.frequency = 0.5f;    // 0.5秒ごとに発生
    emitter_.frequencyTime = 0.0f;// タイマー初期化
    emitter_.transform.translate = { 0.0f, 0.0f, 0.0f };
    emitter_.transform.rotate = { 0.0f, 0.0f, 0.0f };
    emitter_.transform.scale = { 1.0f, 1.0f, 1.0f };

    // 分布の設定： -1.0f ～ 1.0f の間の値をランダムに出す
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

    // 色用の乱数分布 (0.0f ～ 1.0f)
    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    // ------------------------------------
    std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

    // 1. Instancingリソースの作成
    UINT size = kParticleCount_ * sizeof(ParticleForGPU);
    instancingResource_ = CreateBufferResource(device, size);
    instancingResource_->Map(0, nullptr, reinterpret_cast<void **>(&instancingData_));

    // 初期化 (単位行列)
    for(uint32_t i = 0; i < kParticleCount_; ++i) {
        instancingData_[i].World = TransformFunctions::MakeIdentity4x4();
        instancingData_[i].WVP = TransformFunctions::MakeIdentity4x4();
    }

    // 2. SRVの作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.NumElements = kParticleCount_;
    srvDesc.Buffer.StructureByteStride = sizeof(TransformMatrix);

    // ストライド(1要素のサイズ)を ParticleForGPU に合わせる
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

    ID3D12DescriptorHeap *srvHeap = SrvManager::GetInstance()->GetSrvDescriptorHeap();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // ハンドルの計算 (指定されたindexの場所を使う)
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU = srvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU = srvHeap->GetGPUDescriptorHandleForHeapStart();
    srvHandleCPU.ptr += descriptorSize * srvIndex;
    srvHandleGPU.ptr += descriptorSize * srvIndex;
    instancingSrvHandleGPU_ = srvHandleGPU;

    device->CreateShaderResourceView(instancingResource_.Get(), &srvDesc, srvHandleCPU);

    // 3. マテリアルの作成
    materialResource_ = CreateBufferResource(device, sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->lightingType = 0;
    materialData_->uvTransform = TransformFunctions::MakeIdentity4x4();
    // materialResource_->Unmap(0, nullptr); // 書き続けるならUnmapしない

    // 4. テクスチャ読み込み
    // コマンドリストが必要なのでParticleCommonから取得するか、TextureManager::Loadのタイミングを考える必要があるが
    // ここでは初期化時にコマンドリストを渡していないため、事前にロード済みであることを前提とするか、
    // InitializeにCommandListを渡すように変更するのが良い。
    // 今回は簡易的にロード処理を呼ぶ (内部でロード済みならハンドルだけ返ってくる)
    textureIndex_ = TextureManager::GetInstance()->Load(textureFilePath, commandList);

    // +x方向に15m/s、範囲は原点中心に -1 ~ 1
    accelerationField_.acceleration = { 15.0f, 0.0f, 0.0f };
    accelerationField_.area.min = { -1.0f, -1.0f, -1.0f };
    accelerationField_.area.max = { 1.0f, 1.0f, 1.0f };
}

bool ParticleManager::IsCollision(const AABB &aabb, const Vector3 &point) {
    return (point.x >= aabb.min.x && point.x <= aabb.max.x &&
            point.y >= aabb.min.y && point.y <= aabb.max.y &&
            point.z >= aabb.min.z && point.z <= aabb.max.z);
}

ParticleData ParticleManager::MakeNewParticle(const Vector3 &translate) {
    ParticleData p;

    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

    // ランダムな移動ベクトル
    p.velocity = { distribution(randomEngine_), distribution(randomEngine_), distribution(randomEngine_) };

    // 位置：エミッタの場所(translate) + ランダムなオフセット
    // これにより「エミッタの場所から」パーティクルが出るようになります
    p.transform.translate = {
        translate.x + distribution(randomEngine_),
        translate.y + distribution(randomEngine_),
        translate.z + distribution(randomEngine_)
    };

    p.transform.scale = { 1.0f, 1.0f, 1.0f };
    p.transform.rotate = { 0.0f, 0.0f, 0.0f };
    p.lifeTime = distTime(randomEngine_);
    p.currentTime = 0.0f;

    return p;
}

std::list<ParticleData> ParticleManager::EmitInternal(const Emitter &emitter) {
    std::list<ParticleData> newParticles;
    for(uint32_t count = 0; count < emitter.count; ++count) {
        // エミッタの位置を渡して生成
        newParticles.push_back(MakeNewParticle(emitter.transform.translate));
    }
    return newParticles;
}

ParticleData ParticleManager::MakeNewParticle() {
    ParticleData p;

    // 分布の設定
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

    // 値をランダムにセット
    p.transform.translate = { distribution(randomEngine_), distribution(randomEngine_), distribution(randomEngine_) };
    p.transform.scale = { 1.0f, 1.0f, 1.0f };
    p.transform.rotate = { 0.0f, 0.0f, 0.0f };
    p.velocity = { distribution(randomEngine_), distribution(randomEngine_), distribution(randomEngine_) };
    p.color = { distColor(randomEngine_), distColor(randomEngine_), distColor(randomEngine_), 1.0f };
    p.lifeTime = distTime(randomEngine_);
    p.currentTime = 0.0f;

    return p;
}

void ParticleManager::Emit(const Emitter &emitter) {
    for(uint32_t count = 0; count < emitter.count; ++count) {
        particles_.push_back(MakeNewParticle(emitter.transform.translate));
    }
}

void ParticleManager::Update() {
}

void ParticleManager::DrawImGui() {
#ifdef USE_IMGUI


    // Emitterの座標をいじる
    // 資料の記述: ImGui::DragFloat3("EmitterTranslate", &emitter.transform.translate.x, 0.01f, -100.0f, 100.0f);

    // ウィンドウが乱立しないよう、CollapsingHeader等で囲むのが一般的ですが、
    // まずは資料通りに実装します。
    ImGui::DragFloat3("EmitterTranslate", &emitter_.transform.translate.x, 0.01f, -100.0f, 100.0f);

    // (おまけ) 頻度もいじれると便利なので追加しておくと良いでしょう
    ImGui::DragFloat("Emitter Frequency", &emitter_.frequency, 0.01f, 0.0f, 10.0f);

    // ■ 追加: ビルボードのON/OFF切り替え
    ImGui::Checkbox("Is Billboard", &isBillboard_);
#endif // USE_IMGUI
}

void ParticleManager::Draw(const Matrix4x4 &viewProjection) {
    ID3D12GraphicsCommandList *commandList = particleCommon_->GetCommandList();

    // GPU転送処理呼び出し (引数減った)
    TransferToGPU(viewProjection);

    particleCommon_->SetBlendMode(blendMode_);

    // 描画前にParticleCommonへモードを設定
    particleCommon_->SetBlendMode(blendMode_);

    // 頂点バッファセット (Commonが持っている板ポリゴン)
    commandList->IASetVertexBuffers(0, 1, &particleCommon_->GetVertexBufferView());

    // RootParam[0]: Material
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // RootParam[1]: Instancing Data (SRV DescriptorTable)
    commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_);

    // RootParam[2]: Texture (SRV DescriptorTable)
    commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetGpuHandle(textureIndex_));

    // RootParam[3]: DirectionalLight (使わない場合はダミーを設定するか、シェーダーで無視する)
    // ※Particle.VS.hlslがDirectionalLightを要求する場合、Scene等から渡されたLightのリソースアドレスが必要になります。
    // 今回は簡易化のため省略するか、別途Setする関数を作る必要があります。

    // インスタンシング描画
    if(numActiveParticles_ > 0) {
        commandList->DrawInstanced(particleCommon_->GetVertexCount(), numActiveParticles_, 0, 0);
    }
}

void ParticleManager::TransferToGPU(const Matrix4x4 &viewProjection) {

    // ★ここで ParticleCommon からカメラ行列をもらう！
    Matrix4x4 cameraMatrix = particleCommon_->GetCameraMatrix();

    numActiveParticles_ = 0;

    // ビルボード行列の計算をフラグで分岐させる
    Matrix4x4 billboardMatrix;

    if(isBillboard_) {
        // ONの場合: カメラの向きに合わせて回転を作る (既存の処理)
        Matrix4x4 backToFrontMatrix = TransformFunctions::MakeRoteYMatrix(std::numbers::pi_v<float>);
        billboardMatrix = TransformFunctions::Multiply(backToFrontMatrix, cameraMatrix);
        billboardMatrix.m[3][0] = 0.0f;
        billboardMatrix.m[3][1] = 0.0f;
        billboardMatrix.m[3][2] = 0.0f;
    } else {
        // OFFの場合: 回転なし (単位行列)
        // ※もしパーティクル個別の回転(it->transform.rotate)を使いたい場合は
        //   ここで個別に計算する必要がありますが、まずは「向かない」状態にします
        billboardMatrix = TransformFunctions::MakeIdentity4x4();
    }

    // 2. 全パーティクルをループしてGPUバッファに書き込む
    for(auto it = particles_.begin(); it != particles_.end(); ++it) {
        if(numActiveParticles_ >= kParticleCount_) break;

        // 行列計算
        Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix(it->transform.scale);
        Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(it->transform.translate);
        Matrix4x4 stateMatrix = TransformFunctions::Multiply(scaleMatrix, billboardMatrix);
        Matrix4x4 worldMatrix = TransformFunctions::Multiply(stateMatrix, translateMatrix);
        Matrix4x4 wvpMatrix = TransformFunctions::Multiply(worldMatrix, viewProjection);

        // データセット
        instancingData_[numActiveParticles_].World = worldMatrix;
        instancingData_[numActiveParticles_].WVP = wvpMatrix;
        instancingData_[numActiveParticles_].color = it->color;

        numActiveParticles_++;
    }
}
