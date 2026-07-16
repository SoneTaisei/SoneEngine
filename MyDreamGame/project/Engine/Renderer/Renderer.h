#pragma once
#include <memory>
#include "Core/Utility/Structs.h"

class DirectXCommon;
class Primitive;
class Sprite;
class Model;
class ParticleManager;
class Object3D;
class PrimitiveObject;
class MeshRendererComponent;
class PrimitiveRendererComponent;

class Renderer {
public:
    static Renderer* GetInstance();

    // 初期化と終了
    void Initialize(DirectXCommon* dxCommon);
    
    // 描画フレーム管理
    void PreDraw();
    void PostDraw();

    // 描画メソッド
    void DrawPrimitive(Primitive* primitive);
    void DrawSprite(Sprite* sprite);
    void DrawModel(Model* model);
    void DrawParticle(ParticleManager* particleManager, const Matrix4x4& viewProjection);

    void DrawObject3D(Object3D* obj);
    void DrawPrimitiveObject(PrimitiveObject* obj);
    void DrawPrimitiveGhost(PrimitiveObject* obj, const EulerTransform& transform, const Material& material);

    void AddMeshComponent(MeshRendererComponent* comp);
    void AddPrimitiveComponent(PrimitiveRendererComponent* comp);
    
    // 描画実行（登録されたコンポーネントを描画し、リストをクリアする）
    void RenderComponents();

private:
    void DrawMeshRendererComponent(MeshRendererComponent* comp);
    void DrawPrimitiveRendererComponent(PrimitiveRendererComponent* comp);

private:
    std::vector<MeshRendererComponent*> meshComponents_;
    std::vector<PrimitiveRendererComponent*> primitiveComponents_;

private:
    Renderer() = default;
    ~Renderer() = default;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    DirectXCommon* dxCommon_ = nullptr;
};
