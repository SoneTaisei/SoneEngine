#include "ColliderComponent.h"
#include "Collision/CollisionManager.h"
#include "GameObject/GameObject.h"
#include "Component/TransformComponent.h"
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

ColliderComponent::ColliderComponent() {
}

ColliderComponent::~ColliderComponent() {
    CollisionManager::GetInstance()->UnregisterCollider(this);
}

void ColliderComponent::Initialize() {
    CollisionManager::GetInstance()->RegisterCollider(this);
}

void ColliderComponent::Update() {
}


