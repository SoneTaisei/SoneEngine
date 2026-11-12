#pragma once
#ifdef USE_IMGUI
#include <memory>

class SceneManager;
class EditorManager;
class Object3D;
class GameObject;
class ParticleManager;
class PrimitiveObject;

class EditorHierarchy {
public:
    EditorHierarchy() = default;
    ~EditorHierarchy() = default;

    void Draw(bool* pOpen, EditorManager* editorManager, SceneManager* sceneManager);

    Object3D* GetSelectedObject() const { return selectedObject_; }
    void SetSelectedObject(Object3D* obj) { selectedObject_ = obj; }

    std::shared_ptr<GameObject> GetSelectedGameObject() const { return selectedGameObject_; }
    void SetSelectedGameObject(std::shared_ptr<GameObject> obj) { selectedGameObject_ = obj; }

    ParticleManager* GetSelectedParticle() const { return selectedParticle_; }
    void SetSelectedParticle(ParticleManager* particle) { selectedParticle_ = particle; }

    PrimitiveObject* GetSelectedPrimitive() const { return selectedPrimitive_; }
    void SetSelectedPrimitive(PrimitiveObject* prim) { selectedPrimitive_ = prim; }

    bool IsPlayerSelected() const { return isPlayerSelected_; }
    void SetPlayerSelected(bool selected) { isPlayerSelected_ = selected; }

    void ClearSelection() {
        selectedObject_ = nullptr;
        selectedGameObject_ = nullptr;
        selectedParticle_ = nullptr;
        selectedPrimitive_ = nullptr;
        isPlayerSelected_ = false;
    }

private:
    Object3D* selectedObject_ = nullptr;
    std::shared_ptr<GameObject> selectedGameObject_ = nullptr;
    ParticleManager* selectedParticle_ = nullptr;
    PrimitiveObject* selectedPrimitive_ = nullptr;
    bool isPlayerSelected_ = false;
};
#endif
