#pragma once
#include "ParticleManager.h"

// ParticleManagerを継承
class windowParticle : public ParticleManager {
public:
    // 親クラスのUpdateをオーバーライド（上書き）して、独自の動きを作る
    void Update() override;
};