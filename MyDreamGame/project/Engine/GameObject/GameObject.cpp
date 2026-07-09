#include "GameObject.h"

GameObject::GameObject(const std::string& name) : name_(name) {}

void GameObject::Initialize() {
    // Components are initialized when added, but can do post-init here if needed
}

void GameObject::Update() {
    for (auto& comp : components_) {
        comp->Update();
    }
}

void GameObject::Draw() {
    for (auto& comp : components_) {
        comp->Draw();
    }
}

void GameObject::DisplayImGui() {
    for (auto& comp : components_) {
        comp->DisplayImGui();
    }
}
