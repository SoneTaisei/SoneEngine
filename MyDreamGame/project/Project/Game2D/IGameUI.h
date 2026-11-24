#pragma once

class SpriteCommon;

/// <summary>
/// インゲームUIの共通基底クラス
/// </summary>
class IGameUI {
public:
    virtual ~IGameUI() = default;

    virtual void Initialize(SpriteCommon *spriteCommon) = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
    virtual void Reset() {};

};

