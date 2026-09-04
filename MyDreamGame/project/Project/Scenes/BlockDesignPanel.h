#pragma once
#include <string>
#include "Core/Utility/Structs.h"

class MapChip2D;
class Camera;

/// <summary>
/// ブロック設計パネル（エディタ用。USE_IMGUI 時だけ中身がある）
/// ・ゲームビューでどのブロックにもマウスを乗せる／クリックして選べる
/// ・選んだ1枚のプロパティ（パレットの値）を、その1枚だけ上書きして保存できる（MapChip2D の blockOverrides）
/// ・スイッチ／ドアの連動一覧（番号ごとのペア、片方しか無い番号の警告）
/// ・ゲームビューに設計情報を重ねて描く（連動番号、動く床の範囲、警備員の巡回範囲、ドアの開く向き、選択枠）
/// </summary>
class BlockDesignPanel {
public:
    /// <summary>インスペクター内の折りたたみとして毎フレーム呼ぶ（開いている時だけ）</summary>
    static void Draw(MapChip2D* map, Camera* camera, const std::string& stagePath);
    /// <summary>ゲームビューへの重ね描きとマウス選択。折りたたみが閉じていても毎フレーム呼ぶ</summary>
    static void DrawOverlays(MapChip2D* map, Camera* camera);

    /// <summary>ゲームビューのマウス位置をチップ座標に変換（ゲームビューの外なら false）</summary>
    static bool MouseToChip(MapChip2D* map, Camera* camera, int& outX, int& outY);
    /// <summary>ワールド座標（z=0）をゲームビュー上の画面座標に変換</summary>
    static bool WorldToScreen(Camera* camera, const Vector3& world, float& outX, float& outY);
};
