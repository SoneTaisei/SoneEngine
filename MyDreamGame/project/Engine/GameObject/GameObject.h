#pragma once
#include <string>
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include "../Component/IComponent.h"

class GameObject {
public:
    GameObject(const std::string& name = "GameObject");
    ~GameObject() = default;

    void Initialize();
    void Update();
    void Draw();
    void DisplayImGui();

    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        T* rawPtr = comp.get();
        comp->SetGameObject(this);
        components_.push_back(std::move(comp));
        componentMap_[typeid(T)].push_back(rawPtr);
        rawPtr->Initialize();
        return rawPtr;
    }

    /// <summary>
    /// 当たり判定のデータを持ってくる
    /// </summary>
    /// <typeparam name="T"></typeparam>
    /// <returns></returns>
    template<typename T>
    T* GetComponent() {
        auto it = componentMap_.find(typeid(T));
        if (it != componentMap_.end() && !it->second.empty()) {
            return static_cast<T*>(it->second.front());
        }
        // 基底クラスで派生コンポーネントも取れるようにする。
        for (const auto &comp : components_) {
            if (T *casted = dynamic_cast<T *>(comp.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    template<typename T>
    std::vector<T*> GetComponents() {
        std::vector<T*> result;
        auto it = componentMap_.find(typeid(T));
        if (it != componentMap_.end()) {
            for (auto* comp : it->second) {
                result.push_back(static_cast<T*>(comp));
            }
        }
        return result;
    }

    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

private:
    std::string name_;
    std::vector<std::unique_ptr<IComponent>> components_;
    std::unordered_map<std::type_index, std::vector<IComponent*>> componentMap_;
};
