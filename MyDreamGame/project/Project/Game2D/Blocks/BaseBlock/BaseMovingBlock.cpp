#include "BaseMovingBlock.h"
void BaseMovingBlock::SetupMovingCollider() {
    SetupCollider();
    if (GetGameObject()) {
        if (auto *cc = GetGameObject()->GetComponent<ColliderComponent>()) {
            cc->SetIsMoving(true);
            cc->SetVelocity(GetVelocity());
        }
    }
}