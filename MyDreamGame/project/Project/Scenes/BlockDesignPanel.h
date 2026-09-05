#pragma once
#include <string>
#include "Core/Utility/Structs.h"

class MapChip2D;
class Camera;

/// <summary>
/// ブロック設計パネル（エディタ用。USE_IMGUI 時だけ中身がある）
/// ・ゲームビューでどのブロックにもマウスを乗せる／クリックして選べる
/// ・選んだ1枚のプロパティ（パレットの値）を、その1枚だけ上書きして保存できる（MapChip2D の blockOverrides）
/// ・スイッチ／ドアの連動：番号バッジ、マウスを乗せた時の小さな番号パネル、1〜9 キーで番号、ペアを作るモード
/// ・ゲームビューに設計情報を重ねて描く（連動番号、動く床の範囲、警備員の巡回範囲、ドアの開く向き、選択枠）
/// </summary>
class BlockDesignPanel {
public:
    /// <summary>「Block Design」折りたたみの中身（選んだ1枚のプロパティ、重ね描きの切り替え、保存）</summary>
    static void Draw(MapChip2D* map, Camera* camera, const std::string& stagePath);
    /// <summary>「Switch & Door」折りたたみの中身（連動番号の割り当て）。見つけやすいように別の折りたたみで出す</summary>
    static void DrawLinksPanel(MapChip2D* map, Camera* camera, const std::string& stagePath);
    /// <summary>ゲームビューへの重ね描きとマウス選択、番号キー、マウスを乗せた時の番号パネル。折りたたみが閉じていても毎フレーム呼ぶ</summary>
    static void DrawOverlays(MapChip2D* map, Camera* camera);

    /// <summary>他のパネル（崩れる床など）から「未保存の変更がある」を共有する</summary>
    static void MarkUnsaved();
    /// <summary>保存ボタンと未保存の表示（各パネルで共通）</summary>
    static void DrawSaveRow(MapChip2D* map, const std::string& stagePath, const char* id);

    /// <summary>マウス位置をチップ座標に変換（ゲームビューとマップチップ画面の両方に対応。どちらの外でも false）</summary>
    static bool MouseToChip(MapChip2D* map, Camera* camera, int& outX, int& outY);
    /// <summary>ワールド座標（z=0）を、今表示している画面（ゲームビューかマップチップ画面）の画面座標に変換</summary>
    static bool WorldToScreen(Camera* camera, const Vector3& world, float& outX, float& outY);
    /// <summary>今マウスが乗っている画面でクリック選択ができるか（マップチップ画面ではクリックが塗りになるので不可）</summary>
    static bool CanClickSelect();
    /// <summary>
    /// 実際に画面を描いたカメラのビュー射影行列を渡す（GameScene::Draw で毎フレーム）。
    /// マップチップ画面は専用の2Dカメラ、ゲームビューはゲームカメラかデバッグカメラで描かれるので、
    /// シーンのカメラではなく「描画に使われた行列」で位置を合わせる
    /// </summary>
    static void SetRenderViewProjection(const Matrix4x4& viewProjection);
};
