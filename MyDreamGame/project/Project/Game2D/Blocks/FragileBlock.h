#pragma once
#include "BaseBlock.h"
#include <vector>

/// <summary>
/// 鎖の重さで崩れる床
/// 乗った時の鎖の本数が breakWeight_ 以上なら震え始め、breakDuration_ の間だんだん赤くなってから落ちて消える
/// （震えている間はまだ乗れる。急いで渡り切れる余地を残す）。本数が少なければ普通の床として通れる
/// 見分けが付くように前面にひびを入れ、「通れる上限本数」（breakWeight_ - 1）を点で表示する。
/// 今の本数で崩れる時は赤く染めて予告する。エディタからは SetBreakWeight で1枚ずつ上限を変えられる
/// </summary>
class FragileBlock : public BaseBlock {
public:
    FragileBlock(MapChip2D* map, int chipX, int chipY);
    ~FragileBlock() override = default;

    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;
    void Update() override;
    void Draw() override;

    // 震えている間はまだ足場。落ちて消えたら無くなる
    bool IsSolid() const override { return !isDestroyed_; }
    void OnPlayerStand(Player2D* player) override;
    void SetProperties(const nlohmann::json& properties) override;
    void Reset() override;

    // リプレイ対応（崩壊タイマーを保存・復元する）
    bool IsReplayTracked() const override { return true; }
    void CaptureReplayState(std::vector<float>& outCustom) const override;
    void RestoreReplayState(const std::vector<float>& custom) override;

    // --- エディタ用 ---
    /// <summary>崩れる本数（通れる上限 + 1）を実行中に変える。点の数も追従する</summary>
    void SetBreakWeight(int weight);
    int GetBreakWeight() const { return breakWeight_; }
    /// <summary>通れる鎖の上限本数（この本数までは乗れる）</summary>
    int GetPassLimit() const { return breakWeight_ - 1; }
    /// <summary>エディタで選択中（白く強く点滅させる）</summary>
    void SetSelected(bool on) { selected_ = on; }
    /// <summary>ゲームビューでマウスが乗っている（水色に点滅させる）</summary>
    void SetHovered(bool on) { hovered_ = on; }
    /// <summary>全ての崩れる床を点滅で強調する（どれが崩れる床かを見せる）</summary>
    static void SetHighlightAll(bool on) { s_highlightAll = on; }
    static bool IsHighlightAll() { return s_highlightAll; }
    /// <summary>今プレイヤーが持っている鎖の本数（GameScene が毎フレーム渡す。赤い予告表示用。-1 = 不明）</summary>
    static void SetCurrentChainWeight(int units) { s_currentChainWeight = units; }
    /// <summary>デバッグ用：崩れても消えず、震えた後に元に戻る（何度でも試せる。保存には関係しない）</summary>
    static void SetDebugNoBreak(bool on) { s_debugNoBreak = on; }
    static bool IsDebugNoBreak() { return s_debugNoBreak; }
    /// <summary>エディタの「試し本数」。0 以上なら赤い予告をこの本数で判定する（-1 = 実際の本数を使う）</summary>
    static void SetPreviewChainWeight(int units) { s_previewChainWeight = units; }
    static int GetPreviewChainWeight() { return s_previewChainWeight; }

#ifdef USE_IMGUI
    void DrawImGui() override;
#endif

private:
    std::unique_ptr<GameObject> MakePart(const Vector3& scale, const Vector4& color);
    void SyncPips();
    void LayoutDecor(const Vector3& center, float rotZ, bool danger);

    float startX_ = 0.0f;
    float startY_ = 0.0f;
    ID3D12Device* device_ = nullptr;   // 点を後から増やす時に使う（非所有）
    Primitive* boxPrimitive_ = nullptr; // 同上（非所有）

    int breakWeight_ = 4;        // この本数以上の鎖を持って乗ると崩れる（通れるのは breakWeight_ - 1 本まで）
    float breakDuration_ = 0.5f; // 震え始めてから落ちるまでの時間

    bool isBreaking_ = false;
    float breakTimer_ = 0.0f;
    bool selected_ = false;
    bool hovered_ = false;

    Vector4 baseColor_ = {0.4f, 0.4f, 0.4f, 1.0f}; // パレットの色（最初の Update で取り込む）
    bool baseColorCaptured_ = false;

    // 見分け用の飾り（前面）：ひび2本と、通れる上限本数を示す点
    std::vector<std::unique_ptr<GameObject>> cracks_;
    std::vector<std::unique_ptr<GameObject>> pips_;

    static int s_currentChainWeight;
    static int s_previewChainWeight;
    static bool s_highlightAll;
    static bool s_debugNoBreak;
};
