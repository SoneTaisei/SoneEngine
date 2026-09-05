#include "BlockDesignPanel.h"

#ifdef USE_IMGUI
#include <imgui.h>
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
    bool s_panelOpen = false;          // 今フレーム Draw が呼ばれた（折りたたみが開いている）
    bool s_showOverlays = true;
    bool s_overlaysWhilePlaying = false;
    int s_highlightLinkId = -1;
    bool s_unsaved = false;
    std::string s_saveMessage;

    // 連動（スイッチ／ドア）の割り当て
    bool s_pairMode = false;           // ペアを作るモード：スイッチをクリック → ドアをクリック
    int s_pairSwitchX = -1;            // ペアの元になっているスイッチ
    int s_pairSwitchY = -1;
    std::string s_pairMessage;
    bool s_placementEnabled = false;   // 次に置くスイッチ／ドアの番号を決めておく
    int s_placementLinkId = 1;

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
        {"breakWeight", "崩れる本数（通れるのはこの数 - 1）"},
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
            if (type == t.type) return std::string(t.label) + " (" + type + ")";
        }
        return type.empty() ? std::string("(種類不明)") : type;
    }

    // 文字列プロパティのうち選択肢で出すもの
    const char* kAxisOptions[] = {"X", "Y"};
    const char* kDirectionOptions[] = {"Up", "Down", "Left", "Right"};

    bool ComboForKey(const std::string& key, std::string& value) {
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

    // ---- 連動番号のヘルパー ----
    int LinkIdOf(BaseBlock* b) {
        if (auto* s = dynamic_cast<SwitchBlock*>(b)) return s->GetLinkId();
        if (auto* d = dynamic_cast<DoorBlock*>(b)) return d->GetLinkId();
        return -1;
    }

    // このブロックの連動番号を変える（この1枚の上書きとして保存し、即反映）
    void SetLinkId(MapChip2D* map, BaseBlock* b, int id) {
        if (!b) return;
        id = (std::max)(id, 1);
        int x = b->GetChipX();
        int y = b->GetChipY();
        map->SetBlockOverride(x, y, {{"linkId", id}});
        nlohmann::json merged = map->GetPaletteProperties(x, y);
        if (const nlohmann::json* ov = map->GetBlockOverride(x, y)) merged.update(*ov);
        b->SetProperties(merged);
        s_unsaved = true;
    }

    int NextFreeLinkId(MapChip2D* map) {
        int maxId = 0;
        for (const auto& b : map->GetUpdateBlocks()) {
            if (!b || b->IsDestroyed()) continue;
            maxId = (std::max)(maxId, LinkIdOf(b.get()));
        }
        return maxId + 1;
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

    // 「−  N  ＋」の番号入力。変わったら true
    bool LinkIdInput(const char* id, int& value) {
        ImGui::PushID(id);
        bool changed = false;
        if (ImGui::SmallButton("-")) { value -= 1; changed = true; }
        ImGui::SameLine(0.0f, 3.0f);
        ImGui::Text("番号 %d", value);
        ImGui::SameLine(0.0f, 3.0f);
        if (ImGui::SmallButton("+")) { value += 1; changed = true; }
        value = (std::max)(value, 1);
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

    // ---- 選んだ1枚のプロパティ ----
    void DrawSelectedBlock(MapChip2D* map) {
        BaseBlock* block = map->GetBlock(s_selX, s_selY);
        if (!block || block->IsDestroyed()) {
            ImGui::Text("選択中: (%d, %d) にブロックはありません", s_selX, s_selY);
            if (ImGui::SmallButton("選択を解除")) { s_selX = s_selY = -1; }
            return;
        }
        int typeId = static_cast<int>(map->GetChip(s_selX, s_selY));
        std::string typeName = map->GetBlockTypeName(typeId);
        ImGui::Text("選択中: %s  (%d, %d)", TypeLabelFor(typeName).c_str(), s_selX, s_selY);
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

        bool changed = false;
        for (auto& [key, value] : merged.items()) {
            bool overridden = ov && ov->contains(key);
            std::string label = LabelFor(key) + (overridden ? " *" : "");
            ImGui::PushID(key.c_str());
            ImGui::SetNextItemWidth(170.0f);
            if (value.is_boolean()) {
                bool v = value.get<bool>();
                if (ImGui::Checkbox(label.c_str(), &v)) { value = v; changed = true; }
            } else if (value.is_number_integer() && !value.is_number_float()) {
                int v = value.get<int>();
                if (ImGui::InputInt(label.c_str(), &v, 1, 1)) { value = v; changed = true; }
            } else if (value.is_number()) {
                float v = value.get<float>();
                if (ImGui::DragFloat(label.c_str(), &v, 0.05f)) { value = v; changed = true; }
            } else if (value.is_string()) {
                std::string v = value.get<std::string>();
                if (key == "moveAxis" || key == "openDirection") {
                    if (ComboForKey(key, v)) { value = v; changed = true; }
                    ImGui::SameLine(); ImGui::TextUnformatted(label.c_str());
                } else {
                    char buf[128];
                    strncpy_s(buf, sizeof(buf), v.c_str(), _TRUNCATE);
                    if (ImGui::InputText(label.c_str(), buf, sizeof(buf))) { value = std::string(buf); changed = true; }
                }
            }
            if (changed) {
                // この1枚の上書きとして保存し、ブロックにも即反映
                map->SetBlockOverride(s_selX, s_selY, {{key, value}});
                block->SetProperties(merged);
                s_unsaved = true;
                changed = false;
            }
            ImGui::PopID();
        }

        if (!ov) ImGui::BeginDisabled();
        if (ImGui::SmallButton("この1枚の上書きを消してパレットの値に戻す")) {
            map->ClearBlockOverride(s_selX, s_selY);
            block->SetProperties(base);
            s_unsaved = true;
        }
        if (!ov) ImGui::EndDisabled();
    }

    // ---- スイッチ／ドアの連動 ----
    void DrawLinks(MapChip2D* map) {
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

        ImGui::Text("スイッチ／ドアの連動（同じ番号のスイッチを押すと、同じ番号のドアが全部開く）");
        if (switches.empty() && doors.empty()) {
            ImGui::TextDisabled("このステージにスイッチもドアもありません");
            return;
        }

        // 同じ番号のスイッチが複数ある → 片方を押すだけで同じドアが開いてしまう
        std::map<int, int> switchCountById;
        for (auto* s : switches) switchCountById[s->GetLinkId()] += 1;
        bool anyShared = false;
        for (const auto& [id, n] : switchCountById) if (n > 1) anyShared = true;
        if (anyShared) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "同じ番号のスイッチが複数あります。別々のドアを開けたいなら番号を分けてください");
            ImGui::SameLine();
            if (ImGui::SmallButton("スイッチ全部に別々の番号を振る")) {
                // 番号の小さい順に 1, 2, 3... を振り直す。ドアは元の番号と同じ最初のスイッチについて行く
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
                s_pairMessage = "スイッチに 1 から順に番号を振りました。ドアは元の番号の最初のスイッチに付いています。他のスイッチに付け替えるにはペアを作るモードで";
            }
        }

        // ペアを作るモード
        if (ImGui::Checkbox("ペアを作るモード（ゲームビューでスイッチをクリック → 開けたいドアをクリック）", &s_pairMode)) {
            s_pairSwitchX = s_pairSwitchY = -1;
            s_pairMessage.clear();
        }
        if (s_pairMode) {
            ImGui::Indent(12.0f);
            ImGui::TextDisabled("スイッチをクリックすると、その番号が他のスイッチと同じ場合は自動で新しい番号になる。続けてドアをクリックするとそのドアが同じ番号になる");
            if (s_pairSwitchX >= 0) {
                if (auto* sw = dynamic_cast<SwitchBlock*>(map->GetBlock(s_pairSwitchX, s_pairSwitchY))) {
                    ImGui::Text("元のスイッチ: (%d, %d)  番号 %d  → ドアをクリック", s_pairSwitchX, s_pairSwitchY, sw->GetLinkId());
                } else {
                    s_pairSwitchX = s_pairSwitchY = -1;
                }
            } else {
                ImGui::Text("まずスイッチをクリック");
            }
            ImGui::Unindent(12.0f);
        }
        if (!s_pairMessage.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "%s", s_pairMessage.c_str());
        }

        // 次に置くスイッチ／ドアの番号
        if (ImGui::Checkbox("次に置くスイッチ／ドアの番号を決めておく", &s_placementEnabled)) {
            ApplyPlacementLinkId(map);
        }
        ImGui::SameLine();
        if (LinkIdInput("placement", s_placementLinkId)) {
            ApplyPlacementLinkId(map);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("空き番号にする")) {
            s_placementLinkId = NextFreeLinkId(map);
            ApplyPlacementLinkId(map);
        }
        if (s_placementEnabled) {
            ImGui::TextDisabled("有効な間、Switch / Door を塗るとこの番号で置かれる（パレットを増やさなくてよい）");
        }

        // 番号ごとのまとめ
        std::map<int, std::pair<int, int>> counts; // id -> (switches, doors)
        for (auto* s : switches) counts[s->GetLinkId()].first += 1;
        for (auto* d : doors) counts[d->GetLinkId()].second += 1;
        ImGui::Separator();
        ImGui::Text("番号ごと（押すとそのペアが黄色く強調される）");
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

        // 1枚ずつの番号
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
                if (LinkIdInput("id", id)) {
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
}

// ================= public =================

bool BlockDesignPanel::MouseToChip(MapChip2D* map, Camera* camera, int& outX, int& outY) {
    auto* editor = EditorManager::GetInstance();
    if (!editor || !camera || !map) return false;
    if (!editor->IsGameViewHovered()) return false;
    ImVec2 pos = EditorManager::GetGameViewPos();
    ImVec2 size = EditorManager::GetGameViewSize();
    if (size.x <= 1.0f || size.y <= 1.0f) return false;
    ImVec2 m = ImGui::GetIO().MousePos;
    float u = (m.x - pos.x) / size.x;
    float v = (m.y - pos.y) / size.y;
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return false;
    float ndcX = u * 2.0f - 1.0f;
    float ndcY = 1.0f - v * 2.0f;
    Matrix4x4 viewProj = TransformFunctions::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
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
    ImVec2 pos = EditorManager::GetGameViewPos();
    ImVec2 size = EditorManager::GetGameViewSize();
    if (size.x <= 1.0f || size.y <= 1.0f) return false;
    Matrix4x4 viewProj = TransformFunctions::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    Vector3 ndc = TransformFunctions::EulerTransform(world, viewProj);
    outX = pos.x + (ndc.x + 1.0f) * 0.5f * size.x;
    outY = pos.y + (1.0f - ndc.y) * 0.5f * size.y;
    return true;
}

void BlockDesignPanel::Draw(MapChip2D* map, Camera* camera, const std::string& stagePath) {
    (void)camera;
    if (!map) return;
    s_panelOpen = true;

    ImGui::Checkbox("設計情報をゲームビューに重ねる", &s_showOverlays);
    ImGui::SameLine();
    ImGui::Checkbox("プレイ中も", &s_overlaysWhilePlaying);
    ImGui::TextDisabled("連動番号 / 動く床の範囲 / 警備員の巡回範囲 / ドアの開く向き を描く");

    bool debugCam = EditorManager::GetInstance() && EditorManager::GetInstance()->UseDebugCamera();
    if (debugCam) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "デバッグカメラ中はゲームビューのマウス選択と重ね描きの位置が合いません");
    } else if (s_hoverX >= 0) {
        ImGui::Text("ゲームビュー: (%d, %d) にマウス。クリックで選択", s_hoverX, s_hoverY);
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
    DrawLinks(map);

    ImGui::Separator();
    if (ImGui::Button("ステージファイルに保存 (Save)##design")) {
        bool ok = map->SaveToFile(stagePath);
        s_saveMessage = ok ? "保存しました" : "保存に失敗しました";
        if (ok) s_unsaved = false;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", stagePath.c_str());
    if (s_unsaved) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "未保存の変更があります（エディタ側の自動保存でも書き込まれます）");
    } else if (!s_saveMessage.empty()) {
        ImGui::TextDisabled("%s", s_saveMessage.c_str());
    }
}

void BlockDesignPanel::DrawOverlays(MapChip2D* map, Camera* camera) {
    bool panelOpen = s_panelOpen;
    s_panelOpen = false;
    s_hoverX = s_hoverY = -1;
    if (!map || !camera) return;
    auto* editor = EditorManager::GetInstance();
    if (!editor) return;
    ImVec2 viewSize = EditorManager::GetGameViewSize();
    if (viewSize.x <= 1.0f || viewSize.y <= 1.0f) return;
    if (editor->UseDebugCamera()) return; // 変換が合わないので描かない

    // ---- マウス選択（折りたたみが開いている時だけ） ----
    if (panelOpen) {
        int cx = 0, cy = 0;
        if (MouseToChip(map, camera, cx, cy)) {
            BaseBlock* b = map->GetBlock(cx, cy);
            if (b && !b->IsDestroyed()) {
                s_hoverX = cx;
                s_hoverY = cy;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (s_pairMode && (dynamic_cast<SwitchBlock*>(b) || dynamic_cast<DoorBlock*>(b))) {
                        HandlePairClick(map, b);
                        s_selX = cx;
                        s_selY = cy;
                    } else if (s_selX == cx && s_selY == cy) {
                        s_selX = s_selY = -1;
                    } else {
                        s_selX = cx;
                        s_selY = cy;
                    }
                }
            }
        }
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    bool playing = EditorManager::IsPlaying();
    bool showInfo = s_showOverlays && (!playing || s_overlaysWhilePlaying);

    if (showInfo) {
        std::vector<ImVec2> hlSwitches;
        std::vector<ImVec2> hlDoors;

        for (const auto& b : map->GetUpdateBlocks()) {
            if (!b || b->IsDestroyed()) continue;
            AABB2D box = b->GetAABB();
            ImVec2 mn, mx;
            if (!AABBToScreen(camera, box, mn, mx)) continue;
            ImVec2 center((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f);

            if (auto* s = dynamic_cast<SwitchBlock*>(b.get())) {
                bool hl = (s->GetLinkId() == s_highlightLinkId);
                bool isPairSource = (s_pairMode && s->GetChipX() == s_pairSwitchX && s->GetChipY() == s_pairSwitchY);
                DrawBadge(dl, center, IM_COL32(60, 190, 100, 230), s->GetLinkId(), hl || isPairSource);
                if (isPairSource) dl->AddCircle(center, 18.0f, IM_COL32(255, 230, 80, 255), 0, 2.0f);
                if (hl) hlSwitches.push_back(center);
            } else if (auto* d = dynamic_cast<DoorBlock*>(b.get())) {
                bool hl = (d->GetLinkId() == s_highlightLinkId);
                // ドアは閉じた時の枠で描く（開いていると潰れて見えないため）
                AABB2D closed = d->GetClosedAABB();
                ImVec2 cmn, cmx;
                if (AABBToScreen(camera, closed, cmn, cmx)) {
                    dl->AddRect(cmn, cmx, hl ? IM_COL32(255, 230, 80, 255) : IM_COL32(90, 150, 255, 160), 0.0f, 0, hl ? 3.0f : 1.5f);
                    center = ImVec2((cmn.x + cmx.x) * 0.5f, (cmn.y + cmx.y) * 0.5f);
                    float ax = 0.0f, ay = 0.0f;
                    const std::string& dir = d->GetOpenDirection();
                    if (dir == "Up") ay = -1.0f; else if (dir == "Down") ay = 1.0f; else if (dir == "Left") ax = -1.0f; else ax = 1.0f;
                    float len = (std::min)(cmx.x - cmn.x, cmx.y - cmn.y) * 0.4f;
                    DrawArrow(dl, ImVec2(center.x - ax * len, center.y - ay * len), ImVec2(center.x + ax * len, center.y + ay * len), IM_COL32(200, 220, 255, 220), 2.0f);
                    DrawBadge(dl, ImVec2(center.x, cmn.y + 12.0f), IM_COL32(70, 120, 230, 230), d->GetLinkId(), hl);
                } else {
                    DrawBadge(dl, center, IM_COL32(70, 120, 230, 230), d->GetLinkId(), hl);
                }
                if (hl) hlDoors.push_back(center);
            } else if (auto* m = dynamic_cast<MovingBlock*>(b.get())) {
                float w = box.right - box.left;
                float h = box.top - box.bottom;
                float sx = m->GetStartX();
                float sy = m->GetStartY();
                float r = m->GetMoveRange();
                AABB2D travel;
                if (m->GetMoveAxis() == "Y" || m->GetMoveAxis() == "y") {
                    travel = {sx - w * 0.5f, sy + r + h * 0.5f, sx + w * 0.5f, sy - r - h * 0.5f};
                } else {
                    travel = {sx - r - w * 0.5f, sy + h * 0.5f, sx + r + w * 0.5f, sy - h * 0.5f};
                }
                ImVec2 tmn, tmx;
                if (AABBToScreen(camera, travel, tmn, tmx)) {
                    dl->AddRectFilled(tmn, tmx, IM_COL32(255, 160, 40, 40));
                    dl->AddRect(tmn, tmx, IM_COL32(255, 160, 40, 200), 0.0f, 0, 1.5f);
                }
            } else if (auto* g = dynamic_cast<GuardBlock*>(b.get())) {
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

        for (const auto& s : hlSwitches) {
            for (const auto& d : hlDoors) {
                dl->AddLine(s, d, IM_COL32(255, 230, 80, 200), 2.0f);
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
void BlockDesignPanel::DrawOverlays(MapChip2D*, Camera*) {}
#endif
