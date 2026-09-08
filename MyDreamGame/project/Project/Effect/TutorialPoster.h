#pragma once
#include "Core/Utility/Vector3.h"
#include <memory>
#include <string>
#include <vector>

class PrimitiveObject;
struct ID3D12Device;

/// <summary>
/// 操作説明のポスター：コマ送りアニメ（スプライトシート）を壁（背景板の手前）に貼って見せる
/// - 木の板（振り子を回せる足場）の上に置き、プレイヤーが板に近づくと現れ、離れると消える（アルファで滑らかに）
/// - シートは「コマを格子状に並べた 1 枚の PNG」。JSON メタ（texture / cellW / cellH / cols / frames / fps / loop）で読む
/// - コマの切り替えは Material::uvTransform（UV の拡大縮小と平行移動）で行う。Engine もシェーダーも触らない
/// - 表示だけの演出なので状態の記録は無い（リプレイでも同じ条件で出るだけ）
/// </summary>
class TutorialPoster {
public:
    struct Meta {
        std::string texture;
        int cellW = 320;
        int cellH = 180;
        int cols = 8;
        int frames = 1;
        float fps = 12.0f;
        bool loop = true;
        int holdLastFrames = 0; // ループの最後で止めるコマ数
        // 再生順（コマ番号の並び）。同じ番号を続けて書くとそのコマで止まる（文字を読む間）。空なら 0..frames-1 を順に
        std::vector<int> sequence;
    };

    /// <summary>枠の太さ（マス）と色。0 にすると枠を出さない</summary>
    void SetFrame(float margin, const Vector3& color) { frameMargin_ = margin; frameColor_ = color; }

    TutorialPoster();
    ~TutorialPoster(); // PrimitiveObject の完全な型が見える .cpp 側で定義する（unique_ptr の削除子のため）

    /// <summary>メタ JSON を読み、テクスチャと板を作る。失敗したら IsValid() が false</summary>
    void Initialize(ID3D12Device* device, const std::string& metaPath);

    /// <summary>近づく対象（木の板の範囲。ワールド座標）。この範囲からの距離で出す・消すを決める</summary>
    void SetTarget(float minX, float maxX, float minY, float maxY) { tMinX_ = minX; tMaxX_ = maxX; tMinY_ = minY; tMaxY_ = maxY; }
    /// <summary>ポスターの中心と大きさ（ワールド座標。マス単位）</summary>
    void SetPlacement(const Vector3& center, float width, float height);
    /// <summary>出す距離と消す距離（対象の範囲からの距離。消す方を大きくして行き来でちらつかないように）</summary>
    void SetDistances(float showDist, float hideDist) { showDist_ = showDist; hideDist_ = hideDist; }

    /// <summary>毎フレーム。active = false の間は消す（開始前・捕獲中など）</summary>
    void Update(float dt, const Vector3& playerPos, bool active);
    void Draw();

    /// <summary>近づかなくても出しっぱなしにする</summary>
    void SetAlwaysShow(bool v) { alwaysShow_ = v; }
    /// <summary>編集用：遊んでいない時でも見せる</summary>
    void SetPreview(bool v) { preview_ = v; }

    bool IsValid() const { return obj_ != nullptr; }
    bool IsVisible() const { return alpha_ > 0.001f; }
    float GetAlpha() const { return alpha_; }
    const Meta& GetMeta() const { return meta_; }
    /// <summary>コマの縦横比（高さ ÷ 幅）</summary>
    float GetAspect() const { return (meta_.cellW > 0) ? static_cast<float>(meta_.cellH) / static_cast<float>(meta_.cellW) : 0.5f; }

private:
    void ApplyFrame(int frame);

    Meta meta_;
    std::unique_ptr<PrimitiveObject> obj_;
    std::unique_ptr<PrimitiveObject> frameObj_; // 映像の外側に出す枠（ステージと見分けるため）
    Vector3 center_ = { 0.0f, 0.0f, 0.0f };
    float width_ = 14.0f;
    float height_ = 7.0f;
    float tMinX_ = 0.0f, tMaxX_ = 0.0f, tMinY_ = 0.0f, tMaxY_ = 0.0f;
    float frameMargin_ = 0.18f;
    Vector3 frameColor_ = { 0.93f, 0.95f, 1.0f };
    float showDist_ = 3.0f;
    float hideDist_ = 5.0f;
    bool shown_ = false;
    bool alwaysShow_ = false;
    bool preview_ = false;
    float alpha_ = 0.0f;
    float time_ = 0.0f;
    int frame_ = -1;
};
