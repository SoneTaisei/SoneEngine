#include "Particle.h"
#include "Graphics/TextureManager.h"
#include "Utility/TransformFunctions.h"
#include <cassert>
#include <random>

Particle::~Particle() {
    if(particleCommon_) {
        particleCommon_->RemoveParticle(this);
    }
}

void Particle::Initialize(ID3D12GraphicsCommandList *commandList,ParticleCommon *particleCommon, uint32_t count, const std::string &textureFilePath, int srvIndex, BlendMode blendMode) {
    particleCommon_ = particleCommon;
    kParticleCount_ = count;
    ID3D12Device *device = particleCommon_->GetDevice();
    blendMode_ = blendMode;

    // --- 乱数生成器の準備 ---
    std::random_device seedGenerator;
    std::mt19937 randomEngine(seedGenerator());

    // 分布の設定： -1.0f ～ 1.0f の間の値をランダムに出す
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

    // 色用の乱数分布 (0.0f ～ 1.0f)
    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    // ------------------------------------
    std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

    // CPU側の粒子データのサイズ確保と初期化
    particles_.resize(kParticleCount_);
    for(uint32_t i = 0; i < kParticleCount_; ++i) {
        // --- ランダムに値を設定 ---
        particles_[i].transform.translate = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
        particles_[i].transform.scale = { 1.0f, 1.0f, 1.0f };
        particles_[i].transform.rotate = { 0.0f, 0.0f, 0.0f };
        particles_[i].velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
        particles_[i].color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };
        particles_[i].lifeTime = distTime(randomEngine);
        particles_[i].currentTime = 0;
        // ------------------------------------
    }


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

    ID3D12DescriptorHeap *srvHeap = TextureManager::GetInstance()->GetSrvDescriptorHeap();
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
}

void Particle::Update(const Matrix4x4 &viewProjection, const Matrix4x4 &cameraMatrix) {
    // 時間の定義 (資料通り固定FPS前提)
    //const float kDeltaTime = 1.0f / 60.0f;
    const float kDeltaTime = 0.0f;

    numActiveParticles_ = 0;

    // 1. 裏面が見えているので反転させる行列 (Y軸でπ回転)
    Matrix4x4 backToFrontMatrix = TransformFunctions::MakeRoteYMatrix(std::numbers::pi_v<float>);

    // 2. カメラの回転を適用する (BillboardMatrixの作成)
    //    ビルボード行列 = 裏面反転行列 * カメラ行列
    Matrix4x4 billboardMatrix = TransformFunctions::Multiply(backToFrontMatrix, cameraMatrix);

    // 3. 平行移動成分を削除する (カメラの回転だけを利用するため)
    billboardMatrix.m[3][0] = 0.0f;
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;

    for(uint32_t i = 0; i < kParticleCount_; ++i) {
        // 寿命チェック
        if(particles_[i].lifeTime <= particles_[i].currentTime) {
            continue; // 死んでいるのでスキップ
        }

        // 1. 速度を加算して位置を更新 (位置 += 速度 * 時間)
        particles_[i].transform.translate.x += particles_[i].velocity.x * kDeltaTime;
        particles_[i].transform.translate.y += particles_[i].velocity.y * kDeltaTime;
        particles_[i].transform.translate.z += particles_[i].velocity.z * kDeltaTime;

        // 時間を進める処理
        particles_[i].currentTime += kDeltaTime;

        // TransformFunctions::MakeScaleMatrix がある前提で書いています
        // もしなければ、すべて1.0の単位行列を作るか、関数を追加してください
        Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix(particles_[i].transform.scale);

        // 平行移動行列
        Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(particles_[i].transform.translate);

        // 1. Scale * Billboard
        Matrix4x4 stateMatrix = TransformFunctions::Multiply(scaleMatrix, billboardMatrix);

        // 2. (Scale * Billboard) * Translate
        // これで「worldMatrix」になります
        Matrix4x4 worldMatrix = TransformFunctions::Multiply(stateMatrix, translateMatrix);

        // 3. ビュープロジェクション行列と掛け合わせる
        Matrix4x4 wvpMatrix = TransformFunctions::Multiply(worldMatrix, viewProjection);

        // 生きているデータをGPUバッファの前から順に詰める
        if(numActiveParticles_ < kParticleCount_) {
            instancingData_[numActiveParticles_].World = worldMatrix;
            instancingData_[numActiveParticles_].WVP = wvpMatrix;
            instancingData_[numActiveParticles_].color = particles_[i].color;

            // カウントを増やす
            numActiveParticles_++;
        }
    }
}

void Particle::Draw() {
    ID3D12GraphicsCommandList *commandList = particleCommon_->GetCommandList();
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