#pragma once
#include "BaseBlock.h"
#include <string>

class MovingBlock : public BaseBlock {
public:
    MovingBlock(MapChip2D* map, int chipX, int chipY);
    ~MovingBlock() override = default;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    
    // 木の板（細い足場）と同じ性質にする。動く床としての機能はそのまま
    // ・上からだけ乗れる。下からは頭が当たらずすり抜けて上がれる
    // ・横からもぶつからない。鎖と宝石も素通りする
    bool IsSolid() const override { return false; }
    bool IsOneWay() const override { return true; }
    // 板の上に立っている間は鎖を回せる（板の下は素通りなので下の空間で自由に振れる）
    bool AllowsChainSpin() const override { return true; }
    bool IsMoving() const override { return true; }
    Vector3 GetVelocity() const override { return currentVelocity_; }
    void OnPlayerStand(Player2D* player) override;
    void SetProperties(const nlohmann::json& properties) override;

    // リプレイ復元用（位相タイマーを保存・復元する）
    void CaptureReplayState(std::vector<float>& outCustom) const override;
    void RestoreReplayState(const std::vector<float>& custom) override;

#ifdef USE_IMGUI
    void DrawImGui() override;
#endif

    // エディタの重ね描き用（動く範囲の表示）
    float GetStartX() const { return startX_; }
    // 板はチップの上端に貼り付くので、置いたチップの中心より少し上にある
    float GetStartY() const { return startY_ + plankOffsetY_; }
    const std::string& GetMoveAxis() const { return moveAxis_; }
    // 板の厚み（チップ高さに対する割合）
    float GetThickness() const { return thickness_; }
    float GetMoveRange() const { return moveRange_; }

private:
    float startX_ = 0.0f;
    float startY_ = 0.0f;
    Vector3 prevPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 deltaPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 currentVelocity_ = {0.0f, 0.0f, 0.0f};

    // ---- 板の形（細い足場と同じ作り） ----
    float thickness_ = 0.2f;     // 板の厚み（チップ高さに対する割合）
    float chipWidth_ = 1.0f;     // 置かれたチップの幅（厚みを変えた時に形を作り直すのに使う）
    float chipHeight_ = 1.0f;    // 置かれたチップの高さ
    float plankOffsetY_ = 0.0f;  // チップの中心から板の中心までの高さ
    void ApplyPlankShape();      // 厚みから見た目と当たり判定の形を作る

    std::string moveAxis_ = "X"; // "X" or "Y"
    float moveRange_ = 3.0f;
    float moveSpeed_ = 2.0f;
    float phase_ = -1.0f;     // 開始位相（周期の何割ずらすか 0〜1）。負なら置いた位置から自動で決める（従来通り）
    float timer_ = 0.0f;      // ゲーム内時刻（ReplayManager の共有クロックと同期する）
    bool hasPrevPosition_ = false; // 初回更新かどうか（速度の跳ね上がり防止）

    // 現在の時刻から座標を求める（時間の純粋な関数にすることで再生・シークでも一致する）
    Vector3 CalcPositionAt(float time) const;
};
