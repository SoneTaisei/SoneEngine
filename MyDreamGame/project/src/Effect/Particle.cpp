#include "Particle.h"
#include "Graphics/TextureManager.h"
#include "Utility/TransformFunctions.h"
#include <cassert>
#include <random>
//#include "imgui.h"

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

ParticleData Particle::MakeNewParticle(const Vector3 &translate) {
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
    p.color = { distColor(randomEngine_), distColor(randomEngine_), distColor(randomEngine_), 1.0f };
    p.lifeTime = distTime(randomEngine_);
    p.currentTime = 0.0f;

    return p;
}

std::list<ParticleData> Particle::EmitInternal(const Emitter &emitter) {
    std::list<ParticleData> newParticles;
    for(uint32_t count = 0; count < emitter.count; ++count) {
        // エミッタの位置を渡して生成
        newParticles.push_back(MakeNewParticle(emitter.transform.translate));
    }
    return newParticles;
}

ParticleData Particle::MakeNewParticle() {
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

void Particle::Emit(uint32_t count) {
    for(uint32_t i = 0; i < count; ++i) {
        particles_.push_back(MakeNewParticle());
    }
}

void Particle::Update(const Matrix4x4 &viewProjection, const Matrix4x4 &cameraMatrix) {
    // 時間の定義 (資料通り固定FPS前提)
    const float kDeltaTime = 1.0f / 60.0f;
    //const float kDeltaTime = 0.0f;

    emitter_.frequencyTime += kDeltaTime; // 時刻を進める

    // 頻度より大きくなったら発生
    if(emitter_.frequency <= emitter_.frequencyTime) {
        // 発生させてリストに結合
        particles_.splice(particles_.end(), EmitInternal(emitter_));

        // 余計に過ぎた時間も加味して時刻をリセット
        emitter_.frequencyTime -= emitter_.frequency;
    }

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

    // ■ 変更: イテレータを使用してリストを回す
    for(auto it = particles_.begin(); it != particles_.end(); ) {

        // ■ 資料にある「寿命チェックして削除」する処理
        if(it->lifeTime <= it->currentTime) {
            it = particles_.erase(it); // 削除し、次のイテレータを取得
            continue; // 以降の処理を飛ばして次のループへ
        }

        // --- 更新処理 ---
        it->transform.translate.x += it->velocity.x * kDeltaTime;
        it->transform.translate.y += it->velocity.y * kDeltaTime;
        it->transform.translate.z += it->velocity.z * kDeltaTime;
        it->currentTime += kDeltaTime;

        // 行列計算
        Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix(it->transform.scale);
        Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(it->transform.translate);
        Matrix4x4 stateMatrix = TransformFunctions::Multiply(scaleMatrix, billboardMatrix);
        Matrix4x4 worldMatrix = TransformFunctions::Multiply(stateMatrix, translateMatrix);
        Matrix4x4 wvpMatrix = TransformFunctions::Multiply(worldMatrix, viewProjection);

        // ■ 資料にある「バッファオーバーラン対策」
        // GPUに送るデータは最大数(kParticleCount_)を超えてはいけない
        if(numActiveParticles_ < kParticleCount_) {
            instancingData_[numActiveParticles_].World = worldMatrix;
            instancingData_[numActiveParticles_].WVP = wvpMatrix;
            instancingData_[numActiveParticles_].color = it->color;
            numActiveParticles_++;
        }

        // 次のパーティクルへ
        ++it;
    }
}

void Particle::DrawImGui() {
    // Emitterの座標をいじる
    // 資料の記述: ImGui::DragFloat3("EmitterTranslate", &emitter.transform.translate.x, 0.01f, -100.0f, 100.0f);

    // ウィンドウが乱立しないよう、CollapsingHeader等で囲むのが一般的ですが、
    // まずは資料通りに実装します。
    ImGui::DragFloat3("EmitterTranslate", &emitter_.transform.translate.x, 0.01f, -100.0f, 100.0f);

    // (おまけ) 頻度もいじれると便利なので追加しておくと良いでしょう
    ImGui::DragFloat("Emitter Frequency", &emitter_.frequency, 0.01f, 0.0f, 10.0f);
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