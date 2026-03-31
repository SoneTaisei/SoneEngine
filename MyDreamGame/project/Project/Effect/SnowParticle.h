#pragma once
#include "Effect/ParticleManager.h"

class SnowParticle : public ParticleManager {
public:
    // 親クラスのUpdateを上書き(オーバーライド)する
    void Update() override;
};

