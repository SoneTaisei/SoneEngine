#pragma once
#include "Core/Utility/Structs.h"
#include <algorithm>

/// <summary>
/// スイッチとドアの連動番号（linkId）から色を決める。
/// 同じ番号のスイッチとドアが同じ色になるので、どれとどれが繋がっているか見ただけで分かる。
/// 番号は 1 から。用意した色より多い場合は先頭に戻って使い回す。
/// </summary>
namespace LinkColor {

    /// <summary>番号ごとの基本色（スイッチ・ドアの元の色）</summary>
    inline Vector4 Base(int linkId) {
        static const Vector4 kTable[] = {
            { 0.90f, 0.25f, 0.25f, 1.0f }, // 1 赤
            { 0.95f, 0.80f, 0.20f, 1.0f }, // 2 黄
            { 0.30f, 0.55f, 0.95f, 1.0f }, // 3 青
            { 0.30f, 0.80f, 0.40f, 1.0f }, // 4 緑
            { 0.72f, 0.42f, 0.92f, 1.0f }, // 5 紫
            { 0.95f, 0.55f, 0.20f, 1.0f }, // 6 橙
            { 0.30f, 0.85f, 0.85f, 1.0f }, // 7 水色
            { 0.95f, 0.55f, 0.75f, 1.0f }, // 8 桃
            { 0.62f, 0.66f, 0.72f, 1.0f }, // 9 灰
        };
        const int count = static_cast<int>(sizeof(kTable) / sizeof(kTable[0]));
        int index = ((linkId - 1) % count + count) % count; // 1 始まり。負の番号でも落ちないように
        return kTable[index];
    }

    /// <summary>基本色を明るくした色（スイッチを押している間・ドアが開いている間）</summary>
    inline Vector4 Bright(int linkId) {
        Vector4 c = Base(linkId);
        return { (std::min)(1.0f, c.x + 0.35f), (std::min)(1.0f, c.y + 0.35f), (std::min)(1.0f, c.z + 0.35f), c.w };
    }

    /// <summary>基本色を暗くした色（ドアの本体。スイッチより落ち着いた色で、同じ色でも見分けが付く）</summary>
    inline Vector4 Dark(int linkId) {
        Vector4 c = Base(linkId);
        return { c.x * 0.62f, c.y * 0.62f, c.z * 0.62f, c.w };
    }

} // namespace LinkColor
