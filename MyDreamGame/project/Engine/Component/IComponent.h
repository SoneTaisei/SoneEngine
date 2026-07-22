#pragma once

class GameObject;

class IComponent {
public:
    virtual ~IComponent() = default;

    virtual void Initialize() {}
    virtual void Update() {}
    virtual void Draw() {}
    virtual void DisplayImGui() {}

    GameObject* GetGameObject() const { return gameObject_; }
    void SetGameObject(GameObject* go) { gameObject_ = go; }

protected:
    GameObject* gameObject_ = nullptr;
};
