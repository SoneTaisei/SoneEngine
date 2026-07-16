#include "ParticleManager.h"
#include "Renderer/Renderer.h"
#include "Graphics/TextureManager.h"
#include "Renderer/SrvManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Core/TimeManager.h"
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

    // --- 荵ｱ謨ｰ逕滓・蝎ｨ縺ｮ貅門ｙ ---
    std::random_device seedGenerator;
    randomEngine_.seed(seedGenerator());

    // 蛻晄悄蛹匁凾縺ｯ繝ｪ繧ｹ繝医ｒ繧ｯ繝ｪ繧｢縺吶ｋ縺縺托ｼ域怙蛻昴・繝代・繝・ぅ繧ｯ繝ｫ0蛟具ｼ・
    particles_.clear();

    // 繧ｨ繝溘ャ繧ｿ縺ｮ繝・ヵ繧ｩ繝ｫ繝郁ｨｭ螳・
    emitter_.count = 3;           // 1蝗槭〒3蛟句・繧・
    emitter_.frequency = 0.5f;    // 0.5遘偵＃縺ｨ縺ｫ逋ｺ逕・
    emitter_.frequencyTime = 0.0f;// 繧ｿ繧､繝槭・蛻晄悄蛹・
    emitter_.transform.translate = { 0.0f, 0.0f, 0.0f };
    emitter_.transform.rotate = { 0.0f, 0.0f, 0.0f };
    emitter_.transform.scale = { 1.0f, 1.0f, 1.0f };

    // 蛻・ｸ・・險ｭ螳夲ｼ・-1.0f ・・1.0f 縺ｮ髢薙・蛟､繧偵Λ繝ｳ繝繝縺ｫ蜃ｺ縺・
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

    // 濶ｲ逕ｨ縺ｮ荵ｱ謨ｰ蛻・ｸ・(0.0f ・・1.0f)
    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    // ------------------------------------
    std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

    // 1. Instancing繝ｪ繧ｽ繝ｼ繧ｹ縺ｮ菴懈・
    UINT size = kParticleCount_ * sizeof(ParticleForGPU);
    instancingResource_ = CreateBufferResource(device, size);
    instancingResource_->Map(0, nullptr, reinterpret_cast<void **>(&instancingData_));

    // 蛻晄悄蛹・(蜊倅ｽ崎｡悟・)
    for(uint32_t i = 0; i < kParticleCount_; ++i) {
        instancingData_[i].World = TransformFunctions::MakeIdentity4x4();
        instancingData_[i].WVP = TransformFunctions::MakeIdentity4x4();
    }

    // 2. SRV縺ｮ菴懈・
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.NumElements = kParticleCount_;
    srvDesc.Buffer.StructureByteStride = sizeof(TransformMatrix);

    // 繧ｹ繝医Λ繧､繝・1隕∫ｴ縺ｮ繧ｵ繧､繧ｺ)繧・ParticleForGPU 縺ｫ蜷医ｏ縺帙ｋ
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

    ID3D12DescriptorHeap *srvHeap = SrvManager::GetInstance()->GetSrvDescriptorHeap();
    UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 繝上Φ繝峨Ν縺ｮ險育ｮ・(謖・ｮ壹＆繧後◆index縺ｮ蝣ｴ謇繧剃ｽｿ縺・
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU = srvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU = srvHeap->GetGPUDescriptorHandleForHeapStart();
    srvHandleCPU.ptr += descriptorSize * srvIndex;
    srvHandleGPU.ptr += descriptorSize * srvIndex;
    instancingSrvHandleGPU_ = srvHandleGPU;

    device->CreateShaderResourceView(instancingResource_.Get(), &srvDesc, srvHandleCPU);

    // 3. 繝槭ユ繝ｪ繧｢繝ｫ縺ｮ菴懈・
    materialResource_ = CreateBufferResource(device, sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->lightingType = 0;
    materialData_->uvTransform = TransformFunctions::MakeIdentity4x4();
    // materialResource_->Unmap(0, nullptr); // 譖ｸ縺咲ｶ壹￠繧九↑繧蔚nmap縺励↑縺・

    // 4. 繝・け繧ｹ繝√Ε隱ｭ縺ｿ霎ｼ縺ｿ
    // 繧ｳ繝槭Φ繝峨Μ繧ｹ繝医′蠢・ｦ√↑縺ｮ縺ｧParticleCommon縺九ｉ蜿門ｾ励☆繧九°縲ゝextureManager::Load縺ｮ繧ｿ繧､繝溘Φ繧ｰ繧定・∴繧句ｿ・ｦ√′縺ゅｋ縺・
    // 縺薙％縺ｧ縺ｯ蛻晄悄蛹匁凾縺ｫ繧ｳ繝槭Φ繝峨Μ繧ｹ繝医ｒ貂｡縺励※縺・↑縺・◆繧√∽ｺ句燕縺ｫ繝ｭ繝ｼ繝画ｸ医∩縺ｧ縺ゅｋ縺薙→繧貞燕謠舌→縺吶ｋ縺九・
    // Initialize縺ｫCommandList繧呈ｸ｡縺吶ｈ縺・↓螟画峩縺吶ｋ縺ｮ縺瑚憶縺・・
    // 莉雁屓縺ｯ邁｡譏鍋噪縺ｫ繝ｭ繝ｼ繝牙・逅・ｒ蜻ｼ縺ｶ (蜀・Κ縺ｧ繝ｭ繝ｼ繝画ｸ医∩縺ｪ繧峨ワ繝ｳ繝峨Ν縺縺題ｿ斐▲縺ｦ縺上ｋ)
    textureIndex_ = TextureManager::GetInstance()->Load(textureFilePath);

    // +x譁ｹ蜷代↓15m/s縲∫ｯ・峇縺ｯ蜴溽せ荳ｭ蠢・↓ -1 ~ 1
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

    // 繝ｩ繝ｳ繝繝縺ｪ遘ｻ蜍輔・繧ｯ繝医Ν
    p.velocity = { distribution(randomEngine_), distribution(randomEngine_), distribution(randomEngine_) };

    // 菴咲ｽｮ・壹お繝溘ャ繧ｿ縺ｮ蝣ｴ謇(translate) + 繝ｩ繝ｳ繝繝縺ｪ繧ｪ繝輔そ繝・ヨ
    // 縺薙ｌ縺ｫ繧医ｊ縲後お繝溘ャ繧ｿ縺ｮ蝣ｴ謇縺九ｉ縲阪ヱ繝ｼ繝・ぅ繧ｯ繝ｫ縺悟・繧九ｈ縺・↓縺ｪ繧翫∪縺・
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
        // 繧ｨ繝溘ャ繧ｿ縺ｮ菴咲ｽｮ繧呈ｸ｡縺励※逕滓・
        newParticles.push_back(MakeNewParticle(emitter.transform.translate));
    }
    return newParticles;
}

ParticleData ParticleManager::MakeNewParticle() {
    ParticleData p;

    // 蛻・ｸ・・險ｭ螳・
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

    // 蛟､繧偵Λ繝ｳ繝繝縺ｫ繧ｻ繝・ヨ
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
        ParticleData p = MakeNewParticle(emitter.transform.translate);
        p.transform.scale = emitter.transform.scale;
        particles_.push_back(p);
    }
}

void ParticleManager::EmitCustom(const Vector3& position, const ParticleProperty& minProp, const ParticleProperty& maxProp, uint32_t count) {
    std::uniform_real_distribution<float> distScaleX(minProp.scale.x, maxProp.scale.x);
    std::uniform_real_distribution<float> distScaleY(minProp.scale.y, maxProp.scale.y);
    std::uniform_real_distribution<float> distScaleZ(minProp.scale.z, maxProp.scale.z);
    std::uniform_real_distribution<float> distRotateX(minProp.rotate.x, maxProp.rotate.x);
    std::uniform_real_distribution<float> distRotateY(minProp.rotate.y, maxProp.rotate.y);
    std::uniform_real_distribution<float> distRotateZ(minProp.rotate.z, maxProp.rotate.z);
    std::uniform_real_distribution<float> distVelX(minProp.velocity.x, maxProp.velocity.x);
    std::uniform_real_distribution<float> distVelY(minProp.velocity.y, maxProp.velocity.y);
    std::uniform_real_distribution<float> distVelZ(minProp.velocity.z, maxProp.velocity.z);
    std::uniform_real_distribution<float> distColorR(minProp.color.x, maxProp.color.x);
    std::uniform_real_distribution<float> distColorG(minProp.color.y, maxProp.color.y);
    std::uniform_real_distribution<float> distColorB(minProp.color.z, maxProp.color.z);
    std::uniform_real_distribution<float> distColorA(minProp.color.w, maxProp.color.w);
    std::uniform_real_distribution<float> distLife(minProp.lifeTime, maxProp.lifeTime);

    for (uint32_t i = 0; i < count; ++i) {
        ParticleData p;
        p.transform.translate = position;
        p.transform.scale = { distScaleX(randomEngine_), distScaleY(randomEngine_), distScaleZ(randomEngine_) };
        p.transform.rotate = { distRotateX(randomEngine_), distRotateY(randomEngine_), distRotateZ(randomEngine_) };
        p.velocity = { distVelX(randomEngine_), distVelY(randomEngine_), distVelZ(randomEngine_) };
        p.color = { distColorR(randomEngine_), distColorG(randomEngine_), distColorB(randomEngine_), distColorA(randomEngine_) };
        p.lifeTime = distLife(randomEngine_);
        p.currentTime = 0.0f;
        particles_.push_back(p);
    }
}

void ParticleManager::Update() {
    float deltaTime = TimeManager::GetInstance().GetDeltaTime();
    for (auto it = particles_.begin(); it != particles_.end(); ) {
        it->currentTime += deltaTime;
        if (it->currentTime >= it->lifeTime) {
            it = particles_.erase(it);
        } else {
            it->transform.translate.x += it->velocity.x * deltaTime;
            it->transform.translate.y += it->velocity.y * deltaTime;
            it->transform.translate.z += it->velocity.z * deltaTime;
            ++it;
        }
    }
}

void ParticleManager::DrawImGui() {
#ifdef USE_IMGUI


    // Emitter縺ｮ蠎ｧ讓吶ｒ縺・§繧・
    // 雉・侭縺ｮ險倩ｿｰ: ImGui::DragFloat3("EmitterTranslate", &emitter.transform.translate.x, 0.01f, -100.0f, 100.0f);

    // 繧ｦ繧｣繝ｳ繝峨え縺御ｹｱ遶九＠縺ｪ縺・ｈ縺・，ollapsingHeader遲峨〒蝗ｲ繧縺ｮ縺御ｸ闊ｬ逧・〒縺吶′縲・
    // 縺ｾ縺壹・雉・侭騾壹ｊ縺ｫ螳溯｣・＠縺ｾ縺吶・
    ImGui::DragFloat3("EmitterTranslate", &emitter_.transform.translate.x, 0.01f, -100.0f, 100.0f);

    // (縺翫∪縺・ 鬆ｻ蠎ｦ繧ゅ＞縺倥ｌ繧九→萓ｿ蛻ｩ縺ｪ縺ｮ縺ｧ霑ｽ蜉縺励※縺翫￥縺ｨ濶ｯ縺・〒縺励ｇ縺・
    ImGui::DragFloat("Emitter Frequency", &emitter_.frequency, 0.01f, 0.0f, 10.0f);

    // 笆 霑ｽ蜉: 繝薙Ν繝懊・繝峨・ON/OFF蛻・ｊ譖ｿ縺・
    ImGui::Checkbox("Is Billboard", &isBillboard_);
#endif // USE_IMGUI
}

void ParticleManager::Draw(const Matrix4x4 &viewProjection) {
    Renderer::GetInstance()->DrawParticle(this, viewProjection);
}

void ParticleManager::TransferToGPU(const Matrix4x4 &viewProjection) {

    // 笘・％縺薙〒 ParticleCommon 縺九ｉ繧ｫ繝｡繝ｩ陦悟・繧偵ｂ繧峨≧・・
    Matrix4x4 cameraMatrix = particleCommon_->GetCameraMatrix();

    numActiveParticles_ = 0;

    // 繝薙Ν繝懊・繝芽｡悟・縺ｮ險育ｮ励ｒ繝輔Λ繧ｰ縺ｧ蛻・ｲ舌＆縺帙ｋ
    Matrix4x4 billboardMatrix;

    if(isBillboard_) {
        // ON縺ｮ蝣ｴ蜷・ 繧ｫ繝｡繝ｩ縺ｮ蜷代″縺ｫ蜷医ｏ縺帙※蝗櫁ｻ｢繧剃ｽ懊ｋ (譌｢蟄倥・蜃ｦ逅・
        Matrix4x4 backToFrontMatrix = TransformFunctions::MakeRoteYMatrix(std::numbers::pi_v<float>);
        billboardMatrix = TransformFunctions::Multiply(backToFrontMatrix, cameraMatrix);
        billboardMatrix.m[3][0] = 0.0f;
        billboardMatrix.m[3][1] = 0.0f;
        billboardMatrix.m[3][2] = 0.0f;
    } else {
        // OFF縺ｮ蝣ｴ蜷・ 蝗櫁ｻ｢縺ｪ縺・(蜊倅ｽ崎｡悟・)
        // 窶ｻ繧ゅ＠繝代・繝・ぅ繧ｯ繝ｫ蛟句挨縺ｮ蝗櫁ｻ｢(it->transform.rotate)繧剃ｽｿ縺・◆縺・ｴ蜷医・
        //   縺薙％縺ｧ蛟句挨縺ｫ險育ｮ励☆繧句ｿ・ｦ√′縺ゅｊ縺ｾ縺吶′縲√∪縺壹・縲悟髄縺九↑縺・咲憾諷九↓縺励∪縺・
        billboardMatrix = TransformFunctions::MakeIdentity4x4();
    }

    // 2. 蜈ｨ繝代・繝・ぅ繧ｯ繝ｫ繧偵Ν繝ｼ繝励＠縺ｦGPU繝舌ャ繝輔ぃ縺ｫ譖ｸ縺崎ｾｼ繧
    for(auto it = particles_.begin(); it != particles_.end(); ++it) {
        if(numActiveParticles_ >= kParticleCount_) break;

        // 陦悟・險育ｮ・
        Matrix4x4 scaleMatrix = TransformFunctions::MakeScaleMatrix(it->transform.scale);
        Matrix4x4 rotateXMatrix = TransformFunctions::MakeRoteXMatrix(it->transform.rotate.x);
        Matrix4x4 rotateYMatrix = TransformFunctions::MakeRoteYMatrix(it->transform.rotate.y);
        Matrix4x4 rotateZMatrix = TransformFunctions::MakeRoteZMatrix(it->transform.rotate.z);
        Matrix4x4 rotateMatrix = TransformFunctions::Multiply(rotateXMatrix, TransformFunctions::Multiply(rotateYMatrix, rotateZMatrix));
        
        Matrix4x4 localMatrix = TransformFunctions::Multiply(scaleMatrix, rotateMatrix);
        Matrix4x4 stateMatrix = TransformFunctions::Multiply(localMatrix, billboardMatrix);
        
        Matrix4x4 translateMatrix = TransformFunctions::MakeTranslateMatrix(it->transform.translate);
        Matrix4x4 worldMatrix = TransformFunctions::Multiply(stateMatrix, translateMatrix);
        Matrix4x4 wvpMatrix = TransformFunctions::Multiply(worldMatrix, viewProjection);

        // 繝・・繧ｿ繧ｻ繝・ヨ
        instancingData_[numActiveParticles_].World = worldMatrix;
        instancingData_[numActiveParticles_].WVP = wvpMatrix;
        instancingData_[numActiveParticles_].color = it->color;

        numActiveParticles_++;
    }
}

