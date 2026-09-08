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

    // 無効化されたコンポーネントは GameObject::Draw() で描画をスキップされる
    // （3Dモデルを設定したブロックの立方体プリミティブを消す用途など）
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

protected:
    GameObject* gameObject_ = nullptr;
    bool enabled_ = true;
};
