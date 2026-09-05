#include "BlockDesignPanel.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <vector>
#include "Editor/EditorManager.h"
#include "Core/Utility/TransformFunctions.h"
#include "Graphics/Camera.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Blocks/BaseBlock.h"
#include "Game2D/Blocks/SwitchBlock.h"
#include "Game2D/Blocks/DoorBlock.h"
#include "Game2D/Blocks/MovingBlock.h"
#include "Game2D/Blocks/GuardBlock.h"

namespace {
    // ---- 状態（エディタ内だけで使う） ----
    int s_selX = -1;
    int s_selY = -1;
    int s_hoverX = -1;
    int s_hoverY = -1;
    bool s_panelOpen = false;          // 今フレームどちらかの折りたたみが開いていた
    bool s_designMode = false;         // ブロック設計モード：ON の間は折りたたみを閉じていても、プレイ中でも、マウスを乗せて設定できる
    bool s_showRanges = false;         // 動く床・警備員の範囲とドアの向きを常に出す（既定 OFF。普段はマウスを乗せたブロックだけ）
    bool s_badgesWhilePlaying = true;  // 連動番号のバッジはプレイ中も出す
    int s_highlightLinkId = -1;
    bool s_unsaved = false;
    std::string s_saveMessage;

    // 連動（スイッチ／ドア）の割り当て
    bool s_pairMode = false;           // ペアを作るモード：スイッチをクリック → ドアをクリック
    int s_pairSwitchX = -1;
    int s_pairSwitchY = -1;
    std::string s_pairMessage;
    bool s_placementEnabled = false;   // 次に置くスイッチ／ドアの番号を決めておく
    int s_placementLinkId = 1;
    bool s_autoNumberSwitches = true;  // 新しく置いたスイッチは自動で空き番号

    // マウスを乗せた時の小パネル
    int s_popX = -1;
    int s_popY = -1;
    ImVec2 s_popPos = ImVec2(0.0f, 0.0f);
    ImVec2 s_popMin = ImVec2(0.0f, 0.0f);
    ImVec2 s_popMax = ImVec2(0.0f, 0.0f);
    float s_popAwayTime = 0.0f;
    int s_candX = -1;                  // 切り替え先の候補（別のブロックの上に少し留まったら切り替える）
    int s_candY = -1;
    float s_candTime = 0.0f;
    std::string s_keyMessage;

    // つながっている同じ種類のブロック（崩れる床の橋、幅のある動く床、長い板）にまとめて適用する
    bool s_applyConnected = true;

    // 停止直後：再生開始時のマップに戻されるので、プレイ中に変えた上書きを何フレームか戻し続ける
    bool s_wasPlaying = false;
    int s_reapplyFrames = 0;

    // 実際に画面を描いたカメラのビュー射影行列（GameScene::Draw から毎フレーム渡される）
    Matrix4x4 s_renderViewProj = TransformFunctions::MakeIdentity4x4();
    bool s_hasRenderViewProj = false;

    // ---- 今表示している画面（ゲームビュー or マップチップ画面）の画像の位置と大きさ ----
    // マップチップ画面は Engine 側が画像の位置を公開していないので、同じ計算（編集モードの1行の下に、16:9 で中央寄せ）で求める
    bool IsMapTabActive() {
        auto* editor = EditorManager::GetInstance();
        return editor && editor->GetActiveMainTab() == "マップチップ画面";
    }

    bool GetViewRect(ImVec2& outPos, ImVec2& outSize) {
        if (IsMapTabActive()) {
            ImGuiWindow* w = ImGui::FindWindowByName("マップチップ画面");
            if (!w || !w->WasActive || w->Hidden) return false;
            ImVec2 start = w->DC.CursorStartPos;
            ImVec2 cursor(start.x, start.y + ImGui::GetFrameHeightWithSpacing()); // 「編集モード」の1行分
            ImVec2 avail(w->ContentRegionRect.Max.x - cursor.x, w->ContentRegionRect.Max.y - cursor.y);
            if (avail.x < 100.0f) avail.x = 100.0f;
            if (avail.y < 100.0f) avail.y = 100.0f;
            const float aspect = 1280.0f / 720.0f;
            ImVec2 size;
            if (avail.x / avail.y > aspect) { size.y = avail.y; size.x = avail.y * aspect; }
            else { size.x = avail.x; size.y = avail.x / aspect; }
            outPos = ImVec2(cursor.x + (avail.x - size.x) * 0.5f, cursor.y + (avail.y - size.y) * 0.5f);
            outSize = size;
            return true;
        }
        outPos = EditorManager::GetGameViewPos();
        outSize = EditorManager::GetGameViewSize();
        return outSize.x > 1.0f && outSize.y > 1.0f;
    }

    bool IsViewHovered() {
        auto* editor = EditorManager::GetInstance();
        if (!editor) return false;
        return IsMapTabActive() ? editor->IsMapEditorHovered() : editor->IsGameViewHovered();
    }

    // 描画に使われた行列を受け取っていれば、どのカメラ（ゲーム／デバッグ／マップ専用）でも位置が合う
    bool IsViewUsable() {
        auto* editor = EditorManager::GetInstance();
        if (!editor) return false;
        return s_hasRenderViewProj;
    }

    // ---- プロパティの日本語ラベル ----
    struct KeyLabel { const char* key; const char* label; };
    const KeyLabel kLabels[] = {
        {"linkId", "連動番号"},
        {"openSpeed", "開く速さ"},
        {"closeSpeed", "閉まる速さ"},
        {"openDirection", "開く向き"},
        {"openDistance", "開く距離（0 = 全部開く）"},
        {"latch", "一度開いたら開いたまま"},
        {"crushKills", "挟まれたらミス（OFF = 通路に鎖があると閉まらない）"},
        {"moveAxis", "動く軸"},
        {"moveRange", "動く範囲（片側）"},
        {"moveSpeed", "速さ"},
        {"phase", "開始位相（-1 = 位置で自動、0〜1）"},
        {"breakWeight", "通れる上限本数（この本数までは乗れる）"},
        {"breakDuration", "震える秒数"},
        {"units", "もらえる鎖の本数"},
        {"thickness", "板の厚み"},
        {"patrolSpeed", "巡回の速さ"},
        {"alertSpeed", "追跡の速さ"},
        {"sightLength", "視界判定の長さ（プレイヤーが見つかる距離）"},
        {"sightHeight", "視界の高さ"},
        {"lightDistance", "ライト照射距離（背景や床を照らす光の届く距離）"},
        {"lightAngleDeg", "ライト照射角度（度数）"},
        {"lightFalloffDeg", "ライト中心輝度角度（度数）"},
        {"lightIntensity", "ライトの明るさ（強度）"},
        {"lightDecay", "ライト減衰率（小さいほど遠くまで明るい）"},
        {"enableShadow", "影の生成（シャドウマッピング）"},
        {"shadowBias", "シャドウバイアス（にじみ防止）"},
        {"shadowIntensity", "影の濃さ（0.0〜1.0）"},
        {"maxAlertGauge", "見つかってからミスまでの秒数"},
        {"startDirection", "初期の向き（1 = 右、-1 = 左）"},
        {"waitTimeAtEdge", "端での待ち秒数"},
        {"stunSpeed", "気絶する宝石の速さ"},
        {"stunBase", "気絶の基本秒数"},
        {"stunPerSpeed", "速さ超過 1 あたりの延長秒数"},
        {"stunMax", "気絶の上限秒数"},
        {"staggerTime", "よろける秒数"},
        {"wakeWarning", "起きる前の予告秒数"},
        {"hitCooldown", "連続ヒット防止の秒数"},
        {"tripStun", "転倒の気絶秒数"},
        {"tripSpeedBonus", "走る速さ 1 あたりの延長秒数"},
        {"tripCooldown", "転倒の連発防止秒数"},
        {"tripFootHeight", "足元判定の高さ"},
        {"unbindStun", "縛りを解かれた後の気絶秒数"},
        {"bindFromBehind", "背後から縛れる"},
        {"investigateSight", "調べに行く見られ秒数（未満なら巡回に戻る）"},
        {"loseSightTime", "追跡を諦めるまでの秒数"},
        {"lookTime", "最後の場所で見回す秒数"},
        {"exposureTime", "追跡中に見られ続けてもう1回発見になる秒数"},
    };

    std::string LabelFor(const std::string& key) {
        for (const auto& kl : kLabels) {
            if (key == kl.key) return std::string(kl.label) + " (" + key + ")";
        }
        return key;
    }

    struct TypeLabel { const char* type; const char* label; };
    const TypeLabel kTypeLabels[] = {
        {"NormalBlock", "ブロック"}, {"DeathBlock", "死ぬ床"}, {"GoalBlock", "ゴール"}, {"OneWayBlock", "一方通行"},
        {"ChainItemBlock", "鎖アイテム"}, {"MovingBlock", "動く床"}, {"FragileBlock", "崩れる床"}, {"SwitchBlock", "スイッチ"},
        {"DoorBlock", "ドア"}, {"GuardBlock", "警備員"}, {"ThinPlatformBlock", "細い足場"}, {"JumpBlock", "ジャンプ台"},
    };

    std::string TypeLabelFor(const std::string& type) {
        for (const auto& t : kTypeLabels) {
            if (type == t.type) return t.label;
        }
        return type.empty() ? std::string("(種類不明)") : type;
    }

    // ---- 種類ごとの「マウスを乗せた時の小パネル」に出す項目と、1〜9 キーで変える項目 ----
    struct QuickDef {
        const char* type;
        std::vector<const char*> keys;   // 小パネルに出す項目（他は Block Design の「選択中」で）
        const char* numberKey;           // 1〜9 キーで変える項目（無ければ nullptr）
        const char* numberHint;          // キーの説明
    };
    const QuickDef kQuickDefs[] = {
        {"SwitchBlock", {"linkId"}, "linkId", "キー 1〜9 = 連動番号 / 0 = 空き番号"},
        {"DoorBlock", {"linkId", "openDirection", "latch", "crushKills"}, "linkId", "キー 1〜9 = 連動番号 / 0 = 空き番号"},
        {"FragileBlock", {"breakWeight", "breakDuration"}, "breakWeight", "キー 0〜8 = 通れる上限本数"},
        {"MovingBlock", {"moveAxis", "moveRange", "moveSpeed", "phase"}, "moveRange", "キー 1〜9 = 動く範囲"},
        {"GuardBlock", {"startDirection", "moveRange", "sightLength", "patrolSpeed"}, "moveRange", "キー 1〜9 = 巡回範囲"},
        {"ChainItemBlock", {"units"}, "units", "キー 1〜8 = もらえる本数"},
        {"ThinPlatformBlock", {"thickness"}, nullptr, nullptr},
    };

    const QuickDef* QuickDefFor(const std::string& type) {
        for (const auto& q : kQuickDefs) {
            if (type == q.type) return &q;
        }
        return nullptr;
    }

    // 文字列プロパティのうち選択肢で出すもの
    const char* kAxisOptions[] = {"X", "Y"};
    const char* kDirectionOptions[] = {"Up", "Down", "Left", "Right"};

    bool ComboForKey(const std::string& key, std::string& value, float width) {
        const char** options = nullptr;
        int count = 0;
        if (key == "moveAxis") { options = kAxisOptions; count = 2; }
        if (key == "openDirection") { options = kDirectionOptions; count = 4; }
        if (!options) return false;
        int current = 0;
        for (int i = 0; i < count; ++i) {
            if (value == options[i]) current = i;
        }
        bool changed = false;
        ImGui::SetNextItemWidth(width);
        if (ImGui::BeginCombo("##combo", options[current])) {
            for (int i = 0; i < count; ++i) {
                bool sel = (i == current);
                if (ImGui::Selectable(options[i], sel)) {
                    value = options[i];
                    changed = true;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool AABBToScreen(Camera* camera, const AABB2D& box, ImVec2& outMin, ImVec2& outMax) {
        float x0, y0, x1, y1;
        if (!BlockDesignPanel::WorldToScreen(camera, {box.left, box.top, 0.0f}, x0, y0)) return false;
        if (!BlockDesignPanel::WorldToScreen(camera, {box.right, box.bottom, 0.0f}, x1, y1)) return false;
        outMin = ImVec2((std::min)(x0, x1), (std::min)(y0, y1));
        outMax = ImVec2((std::max)(x0, x1), (std::max)(y0, y1));
        return true;
    }

    void DrawBadge(ImDrawList* dl, const ImVec2& center, ImU32 fill, int number, bool highlight) {
        float r = highlight ? 13.0f : 10.0f;
        dl->AddCircleFilled(center, r, fill);
        dl->AddCircle(center, r, highlight ? IM_COL32(255, 230, 80, 255) : IM_COL32(0, 0, 0, 200), 0, highlight ? 3.0f : 1.5f);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", number);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), IM_COL32(255, 255, 255, 255), buf);
    }

    void DrawArrow(ImDrawList* dl, const ImVec2& from, const ImVec2& to, ImU32 col, float thickness) {
        dl->AddLine(from, to, col, thickness);
        float dx = to.x - from.x;
        float dy = to.y - from.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-3f) return;
        dx /= len; dy /= len;
        float h = 8.0f;
        ImVec2 a(to.x - dx * h - dy * h * 0.6f, to.y - dy * h + dx * h * 0.6f);
        ImVec2 b(to.x - dx * h + dy * h * 0.6f, to.y - dy * h - dx * h * 0.6f);
        dl->AddTriangleFilled(to, a, b, col);
    }

    // ---- プロパティの読み書き（この1枚の上書きとして保存し、即反映） ----
    std::string TypeNameAt(MapChip2D* map, int x, int y) {
        return map->GetBlockTypeName(static_cast<int>(map->GetChip(x, y)));
    }

    nlohmann::json MergedProps(MapChip2D* map, int x, int y) {
        nlohmann::json merged = map->GetPaletteProperties(x, y);
        if (const nlohmann::json* ov = map->GetBlockOverride(x, y)) merged.update(*ov);
        return merged;
    }

    void SetPropOne(MapChip2D* map, BaseBlock* b, const std::string& key, const nlohmann::json& value) {
        if (!b) return;
        int x = b->GetChipX();
        int y = b->GetChipY();
        map->SetBlockOverride(x, y, {{key, value}});
        b->SetProperties(MergedProps(map, x, y));
        s_unsaved = true;
    }

    // 1チップずつ別ブロックになる種類（崩れる床・動く床・細い足場）は、つながっている同じ種類をひとまとまりとして扱う
    bool IsGroupType(const std::string& type) {
        return type == "FragileBlock" || type == "MovingBlock" || type == "ThinPlatformBlock";
    }

    std::vector<BaseBlock*> ConnectedSameType(MapChip2D* map, BaseBlock* start) {
        std::vector<BaseBlock*> result;
        if (!start) return result;
        std::string type = TypeNameAt(map, start->GetChipX(), start->GetChipY());
        if (!s_applyConnected || !IsGroupType(type)) {
            result.push_back(start);
            return result;
        }
        std::set<std::pair<int, int>> visited;
        std::vector<std::pair<int, int>> stack;
        stack.push_back({start->GetChipX(), start->GetChipY()});
        visited.insert(stack.back());
        while (!stack.empty()) {
            auto [x, y] = stack.back();
            stack.pop_back();
            BaseBlock* b = map->GetBlock(x, y);
            if (!b) continue;
            if (std::find(result.begin(), result.end(), b) == result.end()) result.push_back(b);
            const int dx[4] = {1, -1, 0, 0};
            const int dy[4] = {0, 0, 1, -1};
            for (int i = 0; i < 4; ++i) {
                std::pair<int, int> n = {x + dx[i], y + dy[i]};
                if (visited.count(n)) continue;
                if (TypeNameAt(map, n.first, n.second) != type) continue;
                if (!map->GetBlock(n.first, n.second)) continue;
                visited.insert(n);
                stack.push_back(n);
            }
        }
        return result;
    }

    // 値を変える（まとめて適用が ON で対象の種類なら、つながっている同じ種類にも）
    void SetProp(MapChip2D* map, BaseBlock* b, const std::string& key, const nlohmann::json& value) {
        for (BaseBlock* each : ConnectedSameType(map, b)) {
            SetPropOne(map, each, key, value);
        }
    }

    bool IsLinkBlock(BaseBlock* b) {
        return dynamic_cast<SwitchBlock*>(b) != nullptr || dynamic_cast<DoorBlock*>(b) != nullptr;
    }

    int LinkIdOf(BaseBlock* b) {
        if (auto* s = dynamic_cast<SwitchBlock*>(b)) return s->GetLinkId();
        if (auto* d = dynamic_cast<DoorBlock*>(b)) return d->GetLinkId();
        return -1;
    }

    void SetLinkId(MapChip2D* map, BaseBlock* b, int id) {
        SetProp(map, b, "linkId", (std::max)(id, 1));
    }

    int NextFreeLinkId(MapChip2D* map) {
        return map->GetNextFreeLinkId();
    }

    // 同じ番号のスイッチが他にもあるか
    bool IsSwitchIdShared(MapChip2D* map, SwitchBlock* sw) {
        for (const auto& b : map->GetUpdateBlocks()) {
            if (!b || b->IsDestroyed() || b.get() == sw) continue;
            if (auto* other = dynamic_cast<SwitchBlock*>(b.get())) {
                if (other->GetLinkId() == sw->GetLinkId()) return true;
            }
        }
        return false;
    }

    // 「−  N  ＋」の整数入力。変わったら true
    bool StepInput(const char* id, const char* text, int& value, int minV, int maxV) {
        ImGui::PushID(id);
        bool changed = false;
        if (ImGui::SmallButton("-")) { value -= 1; changed = true; }
        ImGui::SameLine(0.0f, 3.0f);
        ImGui::Text(text, value);
        ImGui::SameLine(0.0f, 3.0f);
        if (ImGui::SmallButton("+")) { value += 1; changed = true; }
        value = std::clamp(value, minV, maxV);
        ImGui::PopID();
        return changed;
    }

    // 1つのプロパティの入力欄（小パネルと「選択中」で共通）。変わったら true
    bool PropertyWidget(const std::string& key, nlohmann::json& value, bool compact) {
        std::string label = LabelFor(key);
        float width = compact ? 90.0f : 170.0f;
        ImGui::PushID(key.c_str());
        bool changed = false;
        if (key == "breakWeight") {
            // 崩れる床は「通れる上限本数」で見せる（内部は +1）
            int limit = value.get<int>() - 1;
            if (StepInput("bw", "通れる上限 %d 本", limit, 0, 8)) { value = limit + 1; changed = true; }
        } else if (key == "linkId") {
            int v = value.get<int>();
            if (StepInput("id", "番号 %d", v, 1, 999)) { value = v; changed = true; }
        } else if (key == "units") {
            int v = value.get<int>();
            if (StepInput("u", "%d 本", v, 1, 8)) { value = v; changed = true; }
        } else if (key == "startDirection") {
            int v = value.get<int>();
            bool right = (v >= 0);
            if (ImGui::RadioButton("右", right)) { value = 1; changed = true; }
            ImGui::SameLine();
            if (ImGui::RadioButton("左", !right)) { value = -1; changed = true; }
            ImGui::SameLine();
            ImGui::TextUnformatted(compact ? "初期の向き" : label.c_str());
        } else if (value.is_boolean()) {
            bool v = value.get<bool>();
            if (ImGui::Checkbox(label.c_str(), &v)) { value = v; changed = true; }
        } else if (value.is_number_integer() && !value.is_number_float()) {
            int v = value.get<int>();
            ImGui::SetNextItemWidth(width);
            if (ImGui::InputInt(label.c_str(), &v, 1, 1)) { value = v; changed = true; }
        } else if (value.is_number()) {
            float v = value.get<float>();
            ImGui::SetNextItemWidth(width);
            if (ImGui::DragFloat(label.c_str(), &v, 0.05f)) { value = v; changed = true; }
        } else if (value.is_string()) {
            std::string v = value.get<std::string>();
            if (key == "moveAxis" || key == "openDirection") {
                if (ComboForKey(key, v, width)) { value = v; changed = true; }
                ImGui::SameLine(); ImGui::TextUnformatted(label.c_str());
            } else {
                char buf[128];
                strncpy_s(buf, sizeof(buf), v.c_str(), _TRUNCATE);
                ImGui::SetNextItemWidth(width);
                if (ImGui::InputText(label.c_str(), buf, sizeof(buf))) { value = std::string(buf); changed = true; }
            }
        }
        ImGui::PopID();
        return changed;
    }

    void ApplyPlacementLinkId(MapChip2D* map) {
        if (s_placementEnabled) {
            map->SetPlacementOverride("SwitchBlock", {{"linkId", s_placementLinkId}});
            map->SetPlacementOverride("DoorBlock", {{"linkId", s_placementLinkId}});
        } else {
            map->ClearPlacementOverride("SwitchBlock");
            map->ClearPlacementOverride("DoorBlock");
        }
    }

    // ペアを作るモードのクリック処理
    void HandlePairClick(MapChip2D* map, BaseBlock* clicked) {
        if (!s_pairMode || !clicked) return;
        char buf[160];
        if (auto* sw = dynamic_cast<SwitchBlock*>(clicked)) {
            if (IsSwitchIdShared(map, sw)) {
                int newId = NextFreeLinkId(map);
                SetLinkId(map, sw, newId);
                snprintf(buf, sizeof(buf), "スイッチ (%d, %d) は他と同じ番号だったので %d にしました。次に開けたいドアをクリック", sw->GetChipX(), sw->GetChipY(), newId);
            } else {
                snprintf(buf, sizeof(buf), "スイッチ (%d, %d) 番号 %d。次に開けたいドアをクリック", sw->GetChipX(), sw->GetChipY(), sw->GetLinkId());
            }
            s_pairSwitchX = sw->GetChipX();
            s_pairSwitchY = sw->GetChipY();
            s_highlightLinkId = sw->GetLinkId();
            s_pairMessage = buf;
        } else if (auto* door = dynamic_cast<DoorBlock*>(clicked)) {
            auto* src = (s_pairSwitchX >= 0) ? dynamic_cast<SwitchBlock*>(map->GetBlock(s_pairSwitchX, s_pairSwitchY)) : nullptr;
            if (!src) {
                s_pairMessage = "先にスイッチをクリックしてください";
                return;
            }
            SetLinkId(map, door, src->GetLinkId());
            s_highlightLinkId = src->GetLinkId();
            snprintf(buf, sizeof(buf), "ドア (%d, %d) を番号 %d にしました。続けて他のドアもクリックできます", door->GetChipX(), door->GetChipY(), src->GetLinkId());
            s_pairMessage = buf;
        }
    }

    // 0〜9 キー：種類ごとの主な項目を変える
    void HandleNumberKeys(MapChip2D* map, BaseBlock* target) {
        if (!target || ImGui::GetIO().WantTextInput) return;
        std::string type = TypeNameAt(map, target->GetChipX(), target->GetChipY());
        const QuickDef* q = QuickDefFor(type);
        if (!q || !q->numberKey) return;
        int pressed = -1;
        for (int k = 0; k <= 9; ++k) {
            if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_0 + k), false) ||
                ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_Keypad0 + k), false)) {
                pressed = k;
            }
        }
        if (pressed < 0) return;

        std::string key = q->numberKey;
        char buf[140];
        const char* typeLabel = TypeLabelFor(type).c_str();
        if (key == "linkId") {
            int id = (pressed == 0) ? NextFreeLinkId(map) : pressed;
            SetLinkId(map, target, id);
            s_highlightLinkId = id;
            snprintf(buf, sizeof(buf), "%s (%d, %d) を番号 %d にしました", typeLabel, target->GetChipX(), target->GetChipY(), id);
        } else if (key == "breakWeight") {
            int limit = std::clamp(pressed, 0, 8);
            SetProp(map, target, key, limit + 1);
            snprintf(buf, sizeof(buf), "%s (%d, %d) の通れる上限を %d 本にしました", typeLabel, target->GetChipX(), target->GetChipY(), limit);
        } else if (key == "units") {
            if (pressed == 0) return;
            int v = std::clamp(pressed, 1, 8);
            SetProp(map, target, key, v);
            snprintf(buf, sizeof(buf), "%s (%d, %d) のもらえる本数を %d にしました", typeLabel, target->GetChipX(), target->GetChipY(), v);
        } else {
            if (pressed == 0) return;
            SetProp(map, target, key, static_cast<float>(pressed));
            snprintf(buf, sizeof(buf), "%s (%d, %d) の %s を %d にしました", typeLabel, target->GetChipX(), target->GetChipY(), LabelFor(key).c_str(), pressed);
        }
        s_keyMessage = buf;
    }

    // マウスを乗せたブロックの横に出す小パネル（種類ごとの主な項目）
    void UpdateHoverPopup(MapChip2D* map, BaseBlock* hovered, bool mouseInGameView) {
        ImGuiIO& io = ImGui::GetIO();
        bool overPopup = false;
        if (s_popX >= 0) {
            ImVec2 m = io.MousePos;
            overPopup = (m.x >= s_popMin.x - 24.0f && m.x <= s_popMax.x + 24.0f && m.y >= s_popMin.y - 24.0f && m.y <= s_popMax.y + 24.0f);
        }

        if (hovered) {
            bool same = (hovered->GetChipX() == s_popX && hovered->GetChipY() == s_popY);
            if (!same) {
                // 別のブロック：小パネルがまだ無ければすぐ出す。あれば少し留まってから切り替える（小パネルへ向かう途中で飛び回らないように）
                bool sameCandidate = (hovered->GetChipX() == s_candX && hovered->GetChipY() == s_candY);
                s_candTime = sameCandidate ? (s_candTime + io.DeltaTime) : 0.0f;
                s_candX = hovered->GetChipX();
                s_candY = hovered->GetChipY();
                if (s_popX < 0 || overPopup == false && s_candTime > 0.35f) {
                    s_popX = hovered->GetChipX();
                    s_popY = hovered->GetChipY();
                    s_popPos = ImVec2(io.MousePos.x + 22.0f, io.MousePos.y + 18.0f);
                    s_candTime = 0.0f;
                }
            } else {
                s_candX = s_candY = -1;
                s_candTime = 0.0f;
            }
            s_popAwayTime = 0.0f;
        } else if (overPopup) {
            s_popAwayTime = 0.0f;
            s_candTime = 0.0f;
        } else {
            s_popAwayTime += io.DeltaTime;
            if ((mouseInGameView && s_popAwayTime > 0.25f) || s_popAwayTime > 0.8f) {
                s_popX = s_popY = -1;
            }
        }
        if (s_popX < 0) return;

        BaseBlock* target = map->GetBlock(s_popX, s_popY);
        std::string type = TypeNameAt(map, s_popX, s_popY);
        const QuickDef* q = QuickDefFor(type);
        if (!target || !q) {
            s_popX = s_popY = -1;
            return;
        }

        ImGui::SetNextWindowPos(s_popPos, ImGuiCond_Always);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("##hoverpopup", nullptr, flags)) {
            ImGui::Text("%s (%d, %d)", TypeLabelFor(type).c_str(), s_popX, s_popY);
            if (target->IsDestroyed()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "消えている");
                ImGui::SameLine();
                if (ImGui::SmallButton("復活")) {
                    for (BaseBlock* each : ConnectedSameType(map, target)) each->Reset();
                }
            }
            size_t groupCount = ConnectedSameType(map, target).size();
            if (groupCount > 1) {
                ImGui::SameLine();
                ImGui::TextDisabled("つながっている %d 枚にまとめて", static_cast<int>(groupCount));
            }
            nlohmann::json merged = MergedProps(map, s_popX, s_popY);
            const nlohmann::json* ov = map->GetBlockOverride(s_popX, s_popY);
            for (const char* k : q->keys) {
                std::string key = k;
                if (!merged.contains(key)) continue;
                nlohmann::json value = merged[key];
                bool overridden = ov && ov->contains(key);
                if (PropertyWidget(key, value, true)) {
                    SetProp(map, target, key, value);
                    if (key == "linkId") s_highlightLinkId = value.get<int>();
                }
                if (overridden) { ImGui::SameLine(); ImGui::TextDisabled("*"); }
                if (key == "linkId") {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("空き番号")) {
                        SetLinkId(map, target, NextFreeLinkId(map));
                        s_highlightLinkId = LinkIdOf(target);
                    }
                }
            }
            if (auto* sw = dynamic_cast<SwitchBlock*>(target)) {
                if (IsSwitchIdShared(map, sw)) {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "他のスイッチと同じ番号");
                }
                if (ImGui::SmallButton(s_pairMode ? "このスイッチにドアを組む（ドアをクリック）" : "ペアを作る（押してからドアをクリック）")) {
                    s_pairMode = true;
                    HandlePairClick(map, target);
                }
            } else if (dynamic_cast<DoorBlock*>(target)) {
                auto* src = (s_pairMode && s_pairSwitchX >= 0) ? dynamic_cast<SwitchBlock*>(map->GetBlock(s_pairSwitchX, s_pairSwitchY)) : nullptr;
                if (src) {
                    char b[64];
                    snprintf(b, sizeof(b), "スイッチ (%d, %d) の番号 %d にする", src->GetChipX(), src->GetChipY(), src->GetLinkId());
                    if (ImGui::SmallButton(b)) HandlePairClick(map, target);
                }
            }
            if (type == "MovingBlock" && groupCount > 1) {
                if (ImGui::SmallButton("この床を一緒に動かす（開始位相をそろえる）")) {
                    SetProp(map, target, "phase", 0.0f);
                }
                if (merged.contains("phase") && merged["phase"].get<float>() < 0.0f) {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "位相が自動のままだと 1 枚ずつずれて動く");
                }
            }
            if (q->numberHint) ImGui::TextDisabled("%s", q->numberHint);
            if (ov) {
                if (ImGui::SmallButton("パレットの値に戻す")) {
                    map->ClearBlockOverride(s_popX, s_popY);
                    target->SetProperties(map->GetPaletteProperties(s_popX, s_popY));
                    s_unsaved = true;
                }
            }
            s_popMin = ImGui::GetWindowPos();
            ImVec2 sz = ImGui::GetWindowSize();
            s_popMax = ImVec2(s_popMin.x + sz.x, s_popMin.y + sz.y);
        }
        ImGui::End();
    }

    // ---- 選んだ1枚のプロパティ（全項目） ----
    void DrawSelectedBlock(MapChip2D* map) {
        BaseBlock* block = map->GetBlock(s_selX, s_selY);
        if (!block) {
            ImGui::Text("選択中: (%d, %d) にブロックはありません", s_selX, s_selY);
            if (ImGui::SmallButton("選択を解除")) { s_selX = s_selY = -1; }
            return;
        }
        std::string typeName = TypeNameAt(map, s_selX, s_selY);
        ImGui::Text("選択中: %s  (%d, %d)", TypeLabelFor(typeName).c_str(), s_selX, s_selY);
        if (block->IsDestroyed()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "消えている");
            ImGui::SameLine();
            if (ImGui::SmallButton("復活")) { block->Reset(); }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("解除")) { s_selX = s_selY = -1; return; }

        nlohmann::json base = map->GetPaletteProperties(s_selX, s_selY);
        const nlohmann::json* ov = map->GetBlockOverride(s_selX, s_selY);
        nlohmann::json merged = base;
        if (ov) merged.update(*ov);

        if (merged.empty()) {
            ImGui::TextDisabled("このブロックには設定項目がありません");
            return;
        }
        ImGui::TextDisabled("値を変えるとこの1枚だけ上書きされる（* = 上書き中）。パレット側の値はインスペクターの「プロパティ」で");

        for (auto& [key, value] : merged.items()) {
            bool overridden = ov && ov->contains(key);
            if (PropertyWidget(key, value, false)) {
                SetProp(map, block, key, value);
            }
            if (overridden) { ImGui::SameLine(); ImGui::TextDisabled("*"); }
        }

        if (!ov) ImGui::BeginDisabled();
        if (ImGui::SmallButton("この1枚の上書きを消してパレットの値に戻す")) {
            map->ClearBlockOverride(s_selX, s_selY);
            block->SetProperties(base);
            s_unsaved = true;
        }
        if (!ov) ImGui::EndDisabled();
    }

    void DrawDesignModeToggle(const char* id) {
        ImGui::PushID(id);
        bool on = s_designMode;
        if (ImGui::Checkbox("ブロック設計モード ON", &on)) {
            s_designMode = on;
        }
        ImGui::SameLine();
        if (s_designMode) {
            ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "プレイ中も、この折りたたみを閉じていても、マウスを乗せて設定できる");
        } else {
            ImGui::TextDisabled("OFF の間はエディタ中（停止中）だけ。プレイ中に使うなら ON にする");
        }
        ImGui::PopID();
    }

    void DrawSaveRow(MapChip2D* map, const std::string& stagePath, const char* id) {
        ImGui::PushID(id);
        if (ImGui::Button("ステージファイルに保存 (Save)")) {
            bool ok = map->SaveToFile(stagePath);
            s_saveMessage = ok ? "保存しました" : "保存に失敗しました";
            if (ok) s_unsaved = false;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", stagePath.c_str());
        if (s_unsaved) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "未保存の変更があります。変更はもう効いていますが、保存しないと次に開いた時に消えます");
        } else if (!s_saveMessage.empty()) {
            ImGui::TextDisabled("%s", s_saveMessage.c_str());
        }
        ImGui::PopID();
    }
}

// ================= public =================

void BlockDesignPanel::MarkUnsaved() {
    s_unsaved = true;
}

void BlockDesignPanel::DrawSaveRow(MapChip2D* map, const std::string& stagePath, const char* id) {
    if (!map) return;
    ::DrawSaveRow(map, stagePath, id);
}

bool BlockDesignPanel::CanClickSelect() {
    return !IsMapTabActive();
}

void BlockDesignPanel::SetRenderViewProjection(const Matrix4x4& viewProjection) {
    s_renderViewProj = viewProjection;
    s_hasRenderViewProj = true;
}

bool BlockDesignPanel::MouseToChip(MapChip2D* map, Camera* camera, int& outX, int& outY) {
    auto* editor = EditorManager::GetInstance();
    if (!editor || !camera || !map) return false;
    if (!IsViewHovered() || !IsViewUsable()) return false;
    ImVec2 pos, size;
    if (!GetViewRect(pos, size)) return false;
    ImVec2 m = ImGui::GetIO().MousePos;
    float u = (m.x - pos.x) / size.x;
    float v = (m.y - pos.y) / size.y;
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return false;
    float ndcX = u * 2.0f - 1.0f;
    float ndcY = 1.0f - v * 2.0f;
    Matrix4x4 viewProj = s_hasRenderViewProj ? s_renderViewProj
                                             : TransformFunctions::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    Matrix4x4 inv = TransformFunctions::Inverse(viewProj);
    Vector3 nearP = TransformFunctions::EulerTransform({ndcX, ndcY, 0.0f}, inv);
    Vector3 farP = TransformFunctions::EulerTransform({ndcX, ndcY, 1.0f}, inv);
    Vector3 dir = {farP.x - nearP.x, farP.y - nearP.y, farP.z - nearP.z};
    if (std::abs(dir.z) < 1e-6f) return false;
    float t = (0.0f - nearP.z) / dir.z;
    float wx = nearP.x + dir.x * t;
    float wy = nearP.y + dir.y * t;
    outX = map->WorldToChipX(wx);
    outY = map->WorldToChipY(wy);
    return true;
}

bool BlockDesignPanel::WorldToScreen(Camera* camera, const Vector3& world, float& outX, float& outY) {
    if (!camera) return false;
    ImVec2 pos, size;
    if (!GetViewRect(pos, size)) return false;
    Matrix4x4 viewProj = s_hasRenderViewProj ? s_renderViewProj
                                             : TransformFunctions::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    Vector3 ndc = TransformFunctions::EulerTransform(world, viewProj);
    outX = pos.x + (ndc.x + 1.0f) * 0.5f * size.x;
    outY = pos.y + (1.0f - ndc.y) * 0.5f * size.y;
    return true;
}

void BlockDesignPanel::DrawLinksPanel(MapChip2D* map, Camera* camera, const std::string& stagePath) {
    (void)camera;
    if (!map) return;
    s_panelOpen = true;

    std::vector<SwitchBlock*> switches;
    std::vector<DoorBlock*> doors;
    for (const auto& b : map->GetUpdateBlocks()) {
        if (!b || b->IsDestroyed()) continue;
        if (auto* s = dynamic_cast<SwitchBlock*>(b.get())) switches.push_back(s);
        else if (auto* d = dynamic_cast<DoorBlock*>(b.get())) doors.push_back(d);
    }
    auto byPos = [](BaseBlock* a, BaseBlock* b) {
        if (a->GetChipY() != b->GetChipY()) return a->GetChipY() < b->GetChipY();
        return a->GetChipX() < b->GetChipX();
    };
    std::sort(switches.begin(), switches.end(), byPos);
    std::sort(doors.begin(), doors.end(), byPos);

    DrawDesignModeToggle("links");
    ImGui::TextWrapped("同じ番号のスイッチを押すと、同じ番号のドアが全部開く。番号はゲームビューで直接変えられる：");
    ImGui::BulletText("スイッチかドアにマウスを乗せると小パネルが出る（− ＋ 空き番号）");
    ImGui::BulletText("マウスを乗せたまま キー 1〜9 でその番号、0 で空き番号");
    ImGui::BulletText("ペアを作る：スイッチをクリック → 開けたいドアをクリック");
    if (!s_keyMessage.empty()) ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "%s", s_keyMessage.c_str());

    if (!IsViewUsable()) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "画面がまだ描かれていません");
    } else if (IsMapTabActive()) {
        ImGui::TextDisabled("マップチップ画面：クリックは塗りになるので、マウスを乗せて小パネルのボタンか数字キーで。選ぶのは Enter");
    }

    if (ImGui::Checkbox("ペアを作るモード", &s_pairMode)) {
        s_pairSwitchX = s_pairSwitchY = -1;
        s_pairMessage.clear();
    }
    ImGui::SameLine();
    if (s_pairMode) {
        if (s_pairSwitchX >= 0) {
            if (auto* sw = dynamic_cast<SwitchBlock*>(map->GetBlock(s_pairSwitchX, s_pairSwitchY))) {
                ImGui::Text("元のスイッチ (%d, %d) 番号 %d → ドアをクリック", s_pairSwitchX, s_pairSwitchY, sw->GetLinkId());
            } else {
                s_pairSwitchX = s_pairSwitchY = -1;
                ImGui::TextDisabled("まずスイッチをクリック");
            }
        } else {
            ImGui::TextDisabled("まずスイッチをクリック（他と同じ番号なら自動で新しい番号になる）");
        }
    } else {
        ImGui::TextDisabled("ON にしてスイッチ → ドアの順にクリック");
    }
    if (!s_pairMessage.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "%s", s_pairMessage.c_str());
    }

    if (ImGui::Checkbox("新しく置いたスイッチは自動で空き番号にする", &s_autoNumberSwitches)) {
        map->SetAutoNumberSwitches(s_autoNumberSwitches);
    }
    if (ImGui::Checkbox("次に置くスイッチ／ドアの番号を決めておく", &s_placementEnabled)) {
        ApplyPlacementLinkId(map);
    }
    ImGui::SameLine();
    if (StepInput("placement", "番号 %d", s_placementLinkId, 1, 999)) {
        ApplyPlacementLinkId(map);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("空き番号にする")) {
        s_placementLinkId = NextFreeLinkId(map);
        ApplyPlacementLinkId(map);
    }
    ImGui::Checkbox("プレイ中も番号を出す", &s_badgesWhilePlaying);

    ImGui::Separator();
    if (switches.empty() && doors.empty()) {
        ImGui::TextDisabled("このステージにスイッチもドアもありません");
        ::DrawSaveRow(map, stagePath, "links");
        return;
    }

    std::map<int, int> switchCountById;
    for (auto* s : switches) switchCountById[s->GetLinkId()] += 1;
    bool anyShared = false;
    for (const auto& [id, n] : switchCountById) if (n > 1) anyShared = true;
    if (anyShared) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "同じ番号のスイッチが複数あります（片方を押すだけで同じドアが開く）");
        ImGui::SameLine();
        if (ImGui::SmallButton("スイッチ全部に別々の番号を振る")) {
            std::map<int, int> firstNewIdForOldId;
            int next = 1;
            for (auto* s : switches) {
                int oldId = s->GetLinkId();
                if (!firstNewIdForOldId.count(oldId)) firstNewIdForOldId[oldId] = next;
                SetLinkId(map, s, next);
                ++next;
            }
            for (auto* d : doors) {
                auto it = firstNewIdForOldId.find(d->GetLinkId());
                if (it != firstNewIdForOldId.end()) SetLinkId(map, d, it->second);
            }
            s_pairMessage = "スイッチに 1 から順に番号を振りました。ドアは元の番号の最初のスイッチに付いています。付け替えはドアにマウスを乗せて番号キー";
        }
    }

    std::map<int, std::pair<int, int>> counts; // id -> (switches, doors)
    for (auto* s : switches) counts[s->GetLinkId()].first += 1;
    for (auto* d : doors) counts[d->GetLinkId()].second += 1;
    ImGui::Text("番号ごと（押すとそのペアの数字が黄色くなる）");
    for (const auto& [id, c] : counts) {
        ImGui::PushID(id);
        bool sel = (s_highlightLinkId == id);
        char label[96];
        snprintf(label, sizeof(label), "番号 %d : スイッチ %d 個 / ドア %d 個", id, c.first, c.second);
        if (ImGui::Selectable(label, sel, 0, ImVec2(260.0f, 0.0f))) {
            s_highlightLinkId = sel ? -1 : id;
        }
        if (c.first == 0 || c.second == 0) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), c.first == 0 ? "スイッチが無い" : "ドアが無い");
        } else if (c.first > 1) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "スイッチが複数");
        }
        ImGui::PopID();
    }

    auto drawRows = [&](const char* title, std::vector<BaseBlock*>& blocks, bool isSwitch) {
        char header[64];
        snprintf(header, sizeof(header), "%s %d 個", title, static_cast<int>(blocks.size()));
        if (!ImGui::TreeNode(header)) return;
        for (auto* b : blocks) {
            ImGui::PushID(b);
            int x = b->GetChipX();
            int y = b->GetChipY();
            bool sel = (s_selX == x && s_selY == y);
            char l[48];
            snprintf(l, sizeof(l), "(%d, %d)", x, y);
            if (ImGui::Selectable(l, sel, 0, ImVec2(80.0f, 0.0f))) {
                if (sel) { s_selX = s_selY = -1; } else { s_selX = x; s_selY = y; }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("押すとゲームビューで白い枠が付く");
            ImGui::SameLine();
            int id = LinkIdOf(b);
            if (StepInput("id", "番号 %d", id, 1, 999)) {
                SetLinkId(map, b, id);
            }
            if (isSwitch) {
                if (auto* sw = dynamic_cast<SwitchBlock*>(b)) {
                    if (IsSwitchIdShared(map, sw)) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "他と同じ");
                        ImGui::SameLine();
                        if (ImGui::SmallButton("空き番号に")) {
                            SetLinkId(map, b, NextFreeLinkId(map));
                        }
                    }
                }
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    };
    std::vector<BaseBlock*> sw(switches.begin(), switches.end());
    std::vector<BaseBlock*> dr(doors.begin(), doors.end());
    drawRows("スイッチ", sw, true);
    drawRows("ドア", dr, false);

    ImGui::Separator();
    ::DrawSaveRow(map, stagePath, "links");
}

void BlockDesignPanel::Draw(MapChip2D* map, Camera* camera, const std::string& stagePath) {
    (void)camera;
    if (!map) return;
    s_panelOpen = true;

    DrawDesignModeToggle("design");
    ImGui::TextWrapped("ゲームビューでブロックにマウスを乗せると、その種類の主な設定が小パネルで出る（崩れる床の上限、動く床の範囲、警備員の向き、鎖アイテムの本数…）。数字キーでも変えられる。全項目は下の「選択中」で");
    ImGui::Checkbox("つながっている同じ種類にまとめて適用（崩れる床の橋 / 幅のある動く床 / 長い板）", &s_applyConnected);
    ImGui::Checkbox("動く床と警備員の範囲を常に出す（普段はマウスを乗せたブロックだけ）", &s_showRanges);
    ImGui::TextDisabled("動く床と警備員は、マウスを乗せると動く範囲が線で出る（両端の短い線が端）");
    if (!s_keyMessage.empty()) ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "%s", s_keyMessage.c_str());

    if (!IsViewUsable()) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "画面がまだ描かれていません");
    } else if (s_hoverX >= 0) {
        ImGui::Text("(%d, %d) にマウス。%s で選択", s_hoverX, s_hoverY, CanClickSelect() ? "クリック" : "Enter");
    } else if (IsMapTabActive()) {
        ImGui::TextDisabled("マップチップ画面：ブロックにマウスを乗せると小パネル。Enter で選択（クリックは塗り）");
    } else {
        ImGui::TextDisabled("ゲームビューでブロックにマウスを乗せると水色の枠、クリックで選択（もう一度押すと解除）");
    }

    ImGui::Separator();
    if (s_selX < 0) {
        ImGui::Text("選択中のブロック: なし");
    } else {
        DrawSelectedBlock(map);
    }

    ImGui::Separator();
    ::DrawSaveRow(map, stagePath, "design");
}

void BlockDesignPanel::DrawOverlays(MapChip2D* map, Camera* camera) {
    bool panelOpen = s_panelOpen;
    s_panelOpen = false;
    s_hoverX = s_hoverY = -1;
    if (!map || !camera) return;
    auto* editor = EditorManager::GetInstance();
    if (!editor) return;

    // ---- プレイ中に変えた上書きを、停止後の読み直しの後で戻す ----
    bool playingNow = EditorManager::IsPlaying();
    map->SetPlaytimeRecording(playingNow);
    if (s_wasPlaying && !playingNow) {
        s_reapplyFrames = 4; // 読み直しは停止の次のフレームに行われるので、数フレーム戻し続ける
    }
    s_wasPlaying = playingNow;
    if (s_reapplyFrames > 0) {
        map->ReapplyPlaytimeOverrides();
        if (--s_reapplyFrames == 0) {
            map->ClearPlaytimeOverrides();
        }
    }

    ImVec2 viewPos, viewSize;
    if (!GetViewRect(viewPos, viewSize)) return;
    if (!IsViewUsable()) return; // 描画行列をまだ受け取っていない

    // ---- マウス（エディタ中はいつでも。プレイ中は折りたたみが開いている時だけ） ----
    BaseBlock* hoveredQuick = nullptr; // 小パネルを出す対象（主な設定がある種類）
    bool mouseInGameView = false;
    bool mouseActive = s_designMode || panelOpen || !EditorManager::IsPlaying();
    if (mouseActive) {
        int cx = 0, cy = 0;
        if (MouseToChip(map, camera, cx, cy)) {
            mouseInGameView = true;
            BaseBlock* b = map->GetBlock(cx, cy);
            if (b) { // 消えたブロック（崩れた床・拾った鎖）も扱えるようにする
                s_hoverX = cx;
                s_hoverY = cy;
                if (QuickDefFor(TypeNameAt(map, cx, cy))) hoveredQuick = b;
                // マップチップ画面ではクリックが塗りになるので、Enter キーで選ぶ
                bool selectNow = CanClickSelect() ? ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                                                  : (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
                if (selectNow) {
                    // 選択はブロックの基準チップで持つ（つながったドアは1つのブロックなので、どのチップを押しても同じ）
                    int bx = b->GetChipX();
                    int by = b->GetChipY();
                    if (s_pairMode && IsLinkBlock(b)) {
                        HandlePairClick(map, b);
                        s_selX = bx;
                        s_selY = by;
                    } else if (s_selX == bx && s_selY == by) {
                        s_selX = s_selY = -1;
                    } else {
                        s_selX = bx;
                        s_selY = by;
                    }
                }
            }
        }
        BaseBlock* keyTarget = hoveredQuick;
        if (!keyTarget && s_popX >= 0) keyTarget = map->GetBlock(s_popX, s_popY);
        HandleNumberKeys(map, keyTarget);
        UpdateHoverPopup(map, hoveredQuick, mouseInGameView);
    } else {
        s_popX = s_popY = -1;
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    bool playing = EditorManager::IsPlaying();
    bool showRanges = s_showRanges && !playing;
    bool showBadges = !playing || s_badgesWhilePlaying;

    // マウスを乗せているブロック（つながっている同じ種類も含む）は動く範囲を出す。小パネルを操作中も出したまま
    std::vector<BaseBlock*> focusBlocks;
    {
        BaseBlock* focus = nullptr;
        if (s_hoverX >= 0) focus = map->GetBlock(s_hoverX, s_hoverY);
        else if (s_popX >= 0) focus = map->GetBlock(s_popX, s_popY);
        if (focus) focusBlocks = ConnectedSameType(map, focus);
    }
    auto isFocused = [&](BaseBlock* b) {
        return std::find(focusBlocks.begin(), focusBlocks.end(), b) != focusBlocks.end();
    };
    // 動く範囲の線（警備員と同じ見た目：中心が動く範囲を線で、両端に短い線）
    auto drawRangeLine = [&](const Vector3& a, const Vector3& bpos, bool vertical, ImU32 col) {
        float x0, y0, x1, y1;
        if (!WorldToScreen(camera, a, x0, y0) || !WorldToScreen(camera, bpos, x1, y1)) return;
        dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), col, 2.0f);
        if (vertical) {
            dl->AddLine(ImVec2(x0 - 6.0f, y0), ImVec2(x0 + 6.0f, y0), col, 2.0f);
            dl->AddLine(ImVec2(x1 - 6.0f, y1), ImVec2(x1 + 6.0f, y1), col, 2.0f);
        } else {
            dl->AddLine(ImVec2(x0, y0 - 6.0f), ImVec2(x0, y0 + 6.0f), col, 2.0f);
            dl->AddLine(ImVec2(x1, y1 - 6.0f), ImVec2(x1, y1 + 6.0f), col, 2.0f);
        }
    };

    if (showRanges || showBadges || !focusBlocks.empty()) {
        for (const auto& b : map->GetUpdateBlocks()) {
            if (!b || b->IsDestroyed()) continue;
            AABB2D box = b->GetAABB();
            ImVec2 mn, mx;
            if (!AABBToScreen(camera, box, mn, mx)) continue;
            ImVec2 center((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f);

            if (auto* s = dynamic_cast<SwitchBlock*>(b.get())) {
                if (!showBadges) continue;
                bool hl = (s->GetLinkId() == s_highlightLinkId);
                bool isPairSource = (s_pairMode && s->GetChipX() == s_pairSwitchX && s->GetChipY() == s_pairSwitchY);
                DrawBadge(dl, center, IM_COL32(60, 190, 100, 230), s->GetLinkId(), hl || isPairSource);
            } else if (auto* d = dynamic_cast<DoorBlock*>(b.get())) {
                if (!showBadges) continue;
                bool hl = (d->GetLinkId() == s_highlightLinkId);
                // 数字だけ。閉じた時の枠の上端に出す（開いていると本体が潰れて見えないため）
                AABB2D closed = d->GetClosedAABB();
                ImVec2 cmn, cmx;
                if (AABBToScreen(camera, closed, cmn, cmx)) {
                    ImVec2 c((cmn.x + cmx.x) * 0.5f, (cmn.y + cmx.y) * 0.5f);
                    if (showRanges) {
                        float ax = 0.0f, ay = 0.0f;
                        const std::string& dir = d->GetOpenDirection();
                        if (dir == "Up") ay = -1.0f; else if (dir == "Down") ay = 1.0f; else if (dir == "Left") ax = -1.0f; else ax = 1.0f;
                        float len = (std::min)(cmx.x - cmn.x, cmx.y - cmn.y) * 0.4f;
                        DrawArrow(dl, ImVec2(c.x - ax * len, c.y - ay * len), ImVec2(c.x + ax * len, c.y + ay * len), IM_COL32(200, 220, 255, 220), 2.0f);
                    }
                    DrawBadge(dl, ImVec2(c.x, cmn.y + 12.0f), IM_COL32(70, 120, 230, 230), d->GetLinkId(), hl);
                } else {
                    DrawBadge(dl, center, IM_COL32(70, 120, 230, 230), d->GetLinkId(), hl);
                }
            } else if (auto* m = dynamic_cast<MovingBlock*>(b.get())) {
                if (!showRanges && !isFocused(b.get())) continue;
                // 置いた位置を中心に、片側 moveRange だけ動く（横軸なら左右、縦軸なら上下）
                float sx = m->GetStartX();
                float sy = m->GetStartY();
                float r = m->GetMoveRange();
                bool vertical = (m->GetMoveAxis() == "Y" || m->GetMoveAxis() == "y");
                ImU32 col = IM_COL32(255, 220, 130, 230);
                if (vertical) {
                    drawRangeLine({sx, sy - r, 0.0f}, {sx, sy + r, 0.0f}, true, col);
                } else {
                    drawRangeLine({sx - r, sy, 0.0f}, {sx + r, sy, 0.0f}, false, col);
                }
            } else if (auto* g = dynamic_cast<GuardBlock*>(b.get())) {
                if (!showRanges && !isFocused(b.get())) continue;
                float gy = g->GetStartY() - (box.top - box.bottom) * 0.5f + 0.15f; // 足元
                float x0, y0, x1, y1;
                if (WorldToScreen(camera, {g->GetStartX() - g->GetMoveRange(), gy, 0.0f}, x0, y0) &&
                    WorldToScreen(camera, {g->GetStartX() + g->GetMoveRange(), gy, 0.0f}, x1, y1)) {
                    ImU32 col = IM_COL32(255, 90, 90, 220);
                    dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), col, 2.0f);
                    dl->AddLine(ImVec2(x0, y0 - 6.0f), ImVec2(x0, y0 + 6.0f), col, 2.0f);
                    dl->AddLine(ImVec2(x1, y1 - 6.0f), ImVec2(x1, y1 + 6.0f), col, 2.0f);
                    float sxp, syp;
                    if (WorldToScreen(camera, {g->GetStartX(), gy, 0.0f}, sxp, syp)) {
                        float d = (g->GetStartDirection() < 0) ? -18.0f : 18.0f;
                        DrawArrow(dl, ImVec2(sxp, syp), ImVec2(sxp + d, syp), col, 2.0f);
                    }

                    // 懐中電灯の照射コーンを描画
                    Vector3 eyePos = g->GetLightPosition();
                    float sxEye, syEye;
                    if (WorldToScreen(camera, eyePos, sxEye, syEye)) {
                        float halfAngle = g->GetLightAngleDeg() * (std::numbers::pi_v<float> / 180.0f);
                        float dir = (g->GetStartDirection() < 0) ? -1.0f : 1.0f;
                        float baseAngle = (dir < 0) ? std::numbers::pi_v<float> : 0.0f;
                        float dist = g->GetSightLength();

                        constexpr int kArcSegs = 8;
                        std::vector<ImVec2> pts;
                        pts.push_back(ImVec2(sxEye, syEye));
                        for (int seg = 0; seg <= kArcSegs; ++seg) {
                            float t = static_cast<float>(seg) / static_cast<float>(kArcSegs);
                            float ang = baseAngle - halfAngle + (halfAngle * 2.0f) * t;
                            Vector3 edgePt = { eyePos.x + std::cos(ang) * dist, eyePos.y + std::sin(ang) * dist, 0.0f };
                            float ex, ey;
                            if (WorldToScreen(camera, edgePt, ex, ey)) {
                                pts.push_back(ImVec2(ex, ey));
                            }
                        }
                        if (pts.size() >= 3) {
                            dl->AddPolyline(pts.data(), static_cast<int>(pts.size()), IM_COL32(255, 230, 80, 160), true, 1.5f);
                            dl->AddConvexPolyFilled(pts.data(), static_cast<int>(pts.size()), IM_COL32(255, 230, 80, 25));
                        }
                    }
                }
            }
        }
    }

    // ---- 選択枠・マウス枠 ----
    auto drawFrame = [&](int x, int y, ImU32 col, float thick) {
        BaseBlock* b = map->GetBlock(x, y);
        if (!b) return;
        AABB2D box = b->GetAABB();
        if (auto* d = dynamic_cast<DoorBlock*>(b)) box = d->GetClosedAABB();
        ImVec2 mn, mx;
        if (AABBToScreen(camera, box, mn, mx)) {
            dl->AddRect(ImVec2(mn.x - 2.0f, mn.y - 2.0f), ImVec2(mx.x + 2.0f, mx.y + 2.0f), col, 0.0f, 0, thick);
        }
    };
    if (s_hoverX >= 0 && !(s_hoverX == s_selX && s_hoverY == s_selY)) {
        drawFrame(s_hoverX, s_hoverY, IM_COL32(120, 220, 255, 255), 2.0f);
    }
    if (s_selX >= 0) {
        float pulse = 0.6f + 0.4f * static_cast<float>(std::sin(ImGui::GetTime() * 8.0));
        drawFrame(s_selX, s_selY, IM_COL32(255, 255, 255, static_cast<int>(255 * pulse)), 3.0f);
    }
}

#else
bool BlockDesignPanel::MouseToChip(MapChip2D*, Camera*, int&, int&) { return false; }
bool BlockDesignPanel::WorldToScreen(Camera*, const Vector3&, float&, float&) { return false; }
void BlockDesignPanel::Draw(MapChip2D*, Camera*, const std::string&) {}
void BlockDesignPanel::DrawLinksPanel(MapChip2D*, Camera*, const std::string&) {}
void BlockDesignPanel::DrawOverlays(MapChip2D*, Camera*) {}
#endif
