#pragma once
#include "Core/Utility/Vector3.h"
#include "Effect/TutorialPoster.h"
#include <memory>
#include <string>
#include <vector>

struct ID3D12Device;
class Camera;

/// <summary>
/// 操作説明の映像（ポスター）をまとめて置く
/// - 1 枚ごとに「どの映像（シート JSON）」「中心の位置」「幅」「出す範囲（プレイヤーがこの範囲に近づくと出る）」を持つ
/// - マップごとの JSON（resources/json/shared/Tutorial/＜マップ名＞_posters.json）に保存する
/// - エディタの ImGui から追加・移動・大きさ・出す範囲を変えられる（ゲームビューに枠を重ねて描く）
/// - 映像そのものは TutorialPoster（スプライトシートのコマ送り）
/// </summary>
class TutorialPosterSet {
public:
    struct Entry {
        std::string name = "説明";
        std::string sheet;                 // メタ JSON のパス（resources/Sprite/anim/xxx.json）
        float x = 0.0f, y = 0.0f;          // ポスターの中心（ワールド座標。マス単位）
        float width = 14.0f;               // 幅（高さはコマの縦横比から決まる）
        float triggerX = 0.0f, triggerY = 0.0f; // 出す範囲の中心
        float triggerW = 6.0f, triggerH = 4.0f; // 出す範囲の大きさ
        float showDist = 3.0f, hideDist = 5.5f; // 範囲からこの距離で出す／消す
        bool alwaysShow = false;           // 近づかなくても出しっぱなし
        std::unique_ptr<TutorialPoster> poster; // 実体（映像）
    };

    /// <summary>マップのファイルパスから保存先の JSON パスを決める</summary>
    static std::string ConfigPathFor(const std::string& mapFilePath);

    /// <summary>保存先を決めて JSON を読む（無ければ空のまま）</summary>
    void Initialize(ID3D12Device* device, const std::string& mapFilePath);
    /// <summary>
    /// マップのパスが変わった時に保存先を付け替える（エディタの停止→再生ではシーン作成時だけ一時ファイル temp_play_map のパスになるため、
    /// 毎フレーム本来のパスを渡してもらい、変わっていたらそのマップの JSON を読み直す）
    /// </summary>
    void RebindMap(const std::string& mapFilePath);
    bool Load();
    bool Save();

    /// <summary>追加して映像を作る。追加した項目を返す</summary>
    Entry& Add(const Entry& src);
    void Remove(size_t index);
    bool Empty() const { return entries_.empty(); }
    size_t Count() const { return entries_.size(); }
    /// <summary>このマップ用の JSON があったか（空の JSON でも true。＝初期配置をしない）</summary>
    bool HasConfigFile() const { return hasFile_; }
    Entry& At(size_t index) { return *entries_[index]; }

    /// <summary>毎フレーム。active = false の間は消す（開始前・捕獲中など）</summary>
    void Update(float dt, const Vector3& playerPos, bool active);
    void Draw();
    /// <summary>ImGui の折りたたみの中身（追加・保存・各項目の編集）とゲームビューへの枠の重ね描き</summary>
    void DrawImGui(Camera* camera, const Vector3& playerPos);

    /// <summary>映像シート（メタ JSON）の一覧（resources/Sprite/anim/*.json）</summary>
    static std::vector<std::string> ListSheets();
    /// <summary>シートの表示名（メタ JSON の "title"、無ければファイル名）</summary>
    static std::string SheetTitle(const std::string& sheetPath);

private:
    void Rebuild(Entry& e);  // 映像を作り直す（シートを変えた時）
    void Apply(Entry& e);    // 位置・大きさ・出す範囲を映像へ反映

    ID3D12Device* device_ = nullptr;
    std::string mapFilePath_;
    std::string configPath_;
    std::vector<std::unique_ptr<Entry>> entries_;
    bool hasFile_ = false;   // JSON を読めた
    bool preview_ = false;   // 編集中：近づかなくても全部見せる
    bool dirty_ = false;     // 未保存の変更がある
    int selected_ = -1;      // 枠を強調する項目
};
