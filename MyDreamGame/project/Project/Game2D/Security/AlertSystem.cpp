#include "AlertSystem.h"
#include "Core/Utility/ParameterManager.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Blocks/GuardBlock.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
    const char* kGroup = "Alert";
    constexpr float kEventLife = 1.6f;   // ポップアップが消えるまでの秒数
    constexpr float kPulseDecay = 4.0f;  // バーの膨らみが戻る速さ
}

AlertSystem* AlertSystem::s_current = nullptr;

AlertSystem::AlertSystem() = default;

AlertSystem::~AlertSystem() {
    if (s_current == this) s_current = nullptr;
}

void AlertSystem::SetAsCurrent(bool on) {
    if (on) {
        s_current = this;
    } else if (s_current == this) {
        s_current = nullptr;
    }
}

void AlertSystem::LoadParams() {
    ParameterManager* pm = ParameterManager::GetInstance();
    params_.strikeEnabled_ = pm->GetValue(kGroup, "strikeEnabled_", true);
    params_.strikeLimit_ = pm->GetValue(kGroup, "strikeLimit_", 3);
    params_.strikeMergeTime_ = pm->GetValue(kGroup, "strikeMergeTime_", 1.0f);
    params_.strikeLimit_ = (std::max)(params_.strikeLimit_, 1);
    params_.enabled_ = pm->GetValue(kGroup, "enabled_", false);
    params_.seenPerSec_ = pm->GetValue(kGroup, "seenPerSec_", 8.0f);
    params_.spottedAdd_ = pm->GetValue(kGroup, "spottedAdd_", 25.0f);
    params_.wakeAdd_ = pm->GetValue(kGroup, "wakeAdd_", 15.0f);
    params_.noiseEnabled_ = pm->GetValue(kGroup, "noiseEnabled_", false);
    params_.noiseAdd_ = pm->GetValue(kGroup, "noiseAdd_", 5.0f);
    params_.noiseRadius_ = pm->GetValue(kGroup, "noiseRadius_", 6.0f);
    params_.noiseSpeed_ = pm->GetValue(kGroup, "noiseSpeed_", 8.0f);
    params_.driftPerSec_ = pm->GetValue(kGroup, "driftPerSec_", 1.0f);
    params_.quietDelay_ = pm->GetValue(kGroup, "quietDelay_", 8.0f);
    params_.quietDecayPerSec_ = pm->GetValue(kGroup, "quietDecayPerSec_", 2.0f);
    params_.captureValue_ = pm->GetValue(kGroup, "captureValue_", 100.0f);
    params_.wakeFarDistance_ = pm->GetValue(kGroup, "wakeFarDistance_", 8.0f);
    params_.wakeFarAdd_ = pm->GetValue(kGroup, "wakeFarAdd_", 5.0f);
    params_.respawnGrace_ = pm->GetValue(kGroup, "respawnGrace_", 3.0f);
    params_.captureValue_ = (std::max)(params_.captureValue_, 1.0f);
}

void AlertSystem::SaveParams() {
    ParameterManager* pm = ParameterManager::GetInstance();
    pm->SetValue(kGroup, "seenPerSec_", params_.seenPerSec_);
    pm->SetValue(kGroup, "spottedAdd_", params_.spottedAdd_);
    pm->SetValue(kGroup, "wakeAdd_", params_.wakeAdd_);
    pm->SetValue(kGroup, "strikeEnabled_", params_.strikeEnabled_);
    pm->SetValue(kGroup, "strikeLimit_", params_.strikeLimit_);
    pm->SetValue(kGroup, "strikeMergeTime_", params_.strikeMergeTime_);
    pm->SetValue(kGroup, "enabled_", params_.enabled_);
    pm->SetValue(kGroup, "noiseEnabled_", params_.noiseEnabled_);
    pm->SetValue(kGroup, "noiseAdd_", params_.noiseAdd_);
    pm->SetValue(kGroup, "noiseRadius_", params_.noiseRadius_);
    pm->SetValue(kGroup, "noiseSpeed_", params_.noiseSpeed_);
    pm->SetValue(kGroup, "driftPerSec_", params_.driftPerSec_);
    pm->SetValue(kGroup, "quietDelay_", params_.quietDelay_);
    pm->SetValue(kGroup, "quietDecayPerSec_", params_.quietDecayPerSec_);
    pm->SetValue(kGroup, "captureValue_", params_.captureValue_);
    pm->SetValue(kGroup, "wakeFarDistance_", params_.wakeFarDistance_);
    pm->SetValue(kGroup, "wakeFarAdd_", params_.wakeFarAdd_);
    pm->SetValue(kGroup, "respawnGrace_", params_.respawnGrace_);
    pm->Save();
}

void AlertSystem::Reset() {
    value_ = 0.0f;
    quietTimer_ = 0.0f;
    captured_ = false;
    guardAlertThisFrame_ = false;
    eventThisFrame_ = false;
    pulse_ = 0.0f;
    strikes_ = 0;
    strikeTime_ = -100.0f;
    clock_ = 0.0f;
    strikePulse_ = 0.0f;
    graceTimer_ = 0.0f;
    seenTimer_ = 0.0f;
    peak_ = 0.0f;
    events_.clear();
    totals_.clear();
    counts_.clear();
}

void AlertSystem::Clamp() {
    if (value_ < 0.0f) value_ = 0.0f;
    if (value_ >= params_.captureValue_) {
        value_ = params_.captureValue_;
        captured_ = true; // 達した瞬間に捕獲
    }
    peak_ = (std::max)(peak_, value_);
}

void AlertSystem::Update(float dt) {
    // ポップアップと膨らみは捕獲後も進める（表示が止まらないように）
    for (auto& e : events_) e.age += dt;
    events_.erase(std::remove_if(events_.begin(), events_.end(), [](const Event& e) { return e.age > kEventLife; }), events_.end());
    pulse_ = (std::max)(0.0f, pulse_ - kPulseDecay * dt);
    strikePulse_ = (std::max)(0.0f, strikePulse_ - kPulseDecay * dt);
    seenTimer_ = (std::max)(0.0f, seenTimer_ - dt);
    clock_ += dt;

    // 復活直後の猶予は回数制でも進める（この間は発見を数えない）
    if (!params_.enabled_ && graceTimer_ > 0.0f && active_ && !captured_) {
        graceTimer_ = (std::max)(0.0f, graceTimer_ - dt);
    }

    if (captured_ || !active_ || !params_.enabled_) {
        guardAlertThisFrame_ = false;
        eventThisFrame_ = false;
        return;
    }

    // 復活直後の猶予：時間経過を止める（加算も Add 側で止まる）
    if (graceTimer_ > 0.0f) {
        graceTimer_ = (std::max)(0.0f, graceTimer_ - dt);
    } else {
        // 時間経過（無策の上限）
        value_ += params_.driftPerSec_ * dt;
    }

    // 静かな時間：どの警備員も Alert でなく、事象も起きていない
    if (guardAlertThisFrame_ || eventThisFrame_) {
        quietTimer_ = 0.0f;
    } else {
        quietTimer_ += dt;
    }
    if (quietTimer_ >= params_.quietDelay_) {
        value_ -= params_.quietDecayPerSec_ * dt;
    }
    Clamp();

    guardAlertThisFrame_ = false;
    eventThisFrame_ = false;
}

void AlertSystem::Add(float amount, const char* reason) {
    if (!params_.enabled_) { counts_[reason ? reason : ""] += 1; return; } // OFF でも回数は数える（クリア評価用）
    if (!active_ || captured_ || amount <= 0.0f || graceTimer_ > 0.0f) return;
    value_ += amount;
    eventThisFrame_ = true;
    quietTimer_ = 0.0f;
    pulse_ = 1.0f;
    std::string name = reason ? reason : "";
    totals_[name] += amount;
    counts_[name] += 1;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s +%d", name.c_str(), static_cast<int>(std::lround(amount)));
    events_.push_back({buf, 0.0f, false});
    if (events_.size() > 4) events_.erase(events_.begin());
    Clamp();
}

void AlertSystem::AddContinuous(float amountPerSec, float dt) {
    if (!params_.enabled_) return;
    if (!active_ || captured_ || amountPerSec <= 0.0f || dt <= 0.0f) return;
    seenTimer_ = 0.25f; // 見られている合図（猶予中でも出す）
    if (graceTimer_ > 0.0f) return;
    value_ += amountPerSec * dt;
    eventThisFrame_ = true;
    quietTimer_ = 0.0f;
    totals_["視認"] += amountPerSec * dt;
    Clamp();
}

void AlertSystem::Notice(const char* text) {
    if (!active_ || captured_ || !text) return;
    events_.push_back({text, 0.0f, true});
    if (events_.size() > 4) events_.erase(events_.begin());
    counts_[text] += 1;
}

void AlertSystem::OnGuardWake(const Vector3& guardPos) {
    float dx = guardPos.x - playerPos_.x;
    float dy = guardPos.y - playerPos_.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist >= params_.wakeFarDistance_) {
        Add(params_.wakeFarAdd_, "通報(遠い)");
    } else {
        Add(params_.wakeAdd_, "通報");
    }
}

void AlertSystem::OnSpotted() {
    if (!active_ || captured_) return;
    // 猶予中は数えない（復活位置で見られて即、を防ぐ）
    if (graceTimer_ > 0.0f) return;

    if (params_.strikeEnabled_) {
        // 同時発見（複数の警備員）は1回にまとめる
        if (clock_ - strikeTime_ < params_.strikeMergeTime_) return;
        strikeTime_ = clock_;
        strikes_ += 1;
        counts_["発見"] += 1;
        strikePulse_ = 1.0f;
        char buf[64];
        snprintf(buf, sizeof(buf), "発見  残り %d 回", (std::max)(0, params_.strikeLimit_ - strikes_));
        events_.push_back({buf, 0.0f, false});
        if (events_.size() > 4) events_.erase(events_.begin());
        if (strikes_ >= params_.strikeLimit_) {
            captured_ = true; // 上限に達した瞬間に捕獲
        }
        return;
    }
    // 値の警戒度
    Add(params_.spottedAdd_, "発見");
}

void AlertSystem::OnExposed() {
    if (!active_ || captured_) return;
    if (params_.strikeEnabled_) {
        strikeTime_ = clock_;
        strikes_ += 1;
        counts_["発見"] += 1;
        strikePulse_ = 1.0f;
        char buf[64];
        snprintf(buf, sizeof(buf), "見られ続けた  残り %d 回", (std::max)(0, params_.strikeLimit_ - strikes_));
        events_.push_back({buf, 0.0f, false});
        if (events_.size() > 4) events_.erase(events_.begin());
        if (strikes_ >= params_.strikeLimit_) captured_ = true;
        return;
    }
    Add(params_.spottedAdd_, "見られ続けた");
}

void AlertSystem::StartGrace(float seconds) {
    graceTimer_ = (std::max)(graceTimer_, seconds);
    quietTimer_ = 0.0f;
}

AlertRank AlertSystem::ComputeRank() const {
    AlertRank r;
    auto count = [&](const char* key) {
        auto it = counts_.find(key);
        return (it == counts_.end()) ? 0 : it->second;
    };
    r.spotted = count("発見");
    r.reported = count("通報") + count("通報(遠い)");
    r.noises = count("騒音");
    r.peak = peak_;
    if (r.spotted == 0 && r.reported == 0) r.rank = 'S';
    else if (r.spotted <= 1 && r.reported <= 1) r.rank = 'A';
    else if (r.spotted <= 2) r.rank = 'B';
    else r.rank = 'C';
    return r;
}

bool AlertSystem::AddNoise(const Vector3& pos, MapChip2D* map, const char* reason) {
    if (!params_.noiseEnabled_) return false; // 騒音はいったん数えない
    if (!active_ || captured_ || !map) return false;
    float r2 = params_.noiseRadius_ * params_.noiseRadius_;
    bool heard = false;
    for (const auto& block : map->GetUpdateBlocks()) {
        auto* guard = dynamic_cast<GuardBlock*>(block.get());
        if (!guard || guard->IsDestroyed() || guard->IsIncapacitated()) continue;
        AABB2D box = guard->GetAABB();
        float gx = (box.left + box.right) * 0.5f;
        float gy = (box.top + box.bottom) * 0.5f;
        float dx = gx - pos.x;
        float dy = gy - pos.y;
        if (dx * dx + dy * dy <= r2) {
            heard = true;
            break;
        }
    }
    if (!heard) return false;
    Add(params_.noiseAdd_, reason ? reason : "騒音");
    return true;
}

// ===== リプレイ =====

void AlertSystem::CaptureReplayObjects(std::vector<ReplayObjectState>& out) {
    ReplayObjectState s;
    s.id = 1;
    s.custom = {value_, quietTimer_, captured_ ? 1.0f : 0.0f, static_cast<float>(strikes_), strikeTime_, clock_};
    out.push_back(s);
}

void AlertSystem::RestoreReplayObjects(const std::vector<ReplayObjectState>& states) {
    for (const auto& s : states) {
        if (s.id != 1 || s.custom.size() < 3) continue;
        value_ = s.custom[0];
        quietTimer_ = s.custom[1];
        captured_ = (s.custom[2] != 0.0f);
        if (s.custom.size() >= 6) {
            strikes_ = static_cast<int>(s.custom[3]);
            strikeTime_ = s.custom[4];
            clock_ = s.custom[5];
        }
    }
}

// ===== ImGui =====

void AlertSystem::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::SeparatorText("回数制（既定）");
    bool strikeChanged = false;
    strikeChanged |= ImGui::Checkbox("回数制を使う (strikeEnabled_)", &params_.strikeEnabled_);
    strikeChanged |= ImGui::InputInt("見つかっていい回数 (strikeLimit_)", &params_.strikeLimit_, 1, 1);
    strikeChanged |= ImGui::DragFloat("同時発見をまとめる秒数 (strikeMergeTime_)", &params_.strikeMergeTime_, 0.1f, 0.0f, 5.0f);
    params_.strikeLimit_ = (std::max)(params_.strikeLimit_, 1);
    if (strikeChanged) {
        ParameterManager* pm = ParameterManager::GetInstance();
        pm->SetValue(kGroup, "strikeEnabled_", params_.strikeEnabled_);
        pm->SetValue(kGroup, "strikeLimit_", params_.strikeLimit_);
        pm->SetValue(kGroup, "strikeMergeTime_", params_.strikeMergeTime_);
    }
    ImGui::Text("発見 %d / %d 回  %s", strikes_, params_.strikeLimit_, captured_ ? "[捕獲]" : "");
    ImGui::SameLine();
    if (ImGui::SmallButton("発見テスト")) { OnSpotted(); }

    ImGui::SeparatorText("値の警戒度（ハードモード用）");
    bool changedEnabled = ImGui::Checkbox("警戒度を使う (enabled_)  OFF = 値は動かず HUD も出ない", &params_.enabled_);
    if (changedEnabled) {
        ParameterManager::GetInstance()->SetValue(kGroup, "enabled_", params_.enabled_);
    }
    ImGui::Text("警戒度 %.1f / %.0f  %s  静か %.1f 秒  %s  最大 %.0f", value_, params_.captureValue_,
                captured_ ? "[捕獲]" : "", quietTimer_, active_ ? "(動作中)" : "(停止中)", peak_);
    if (graceTimer_ > 0.0f) ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "復活の猶予 %.1f 秒", graceTimer_);
    if (seenTimer_ > 0.0f) ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "見られている");
    ImGui::ProgressBar(GetRatio(), ImVec2(-1.0f, 0.0f), "");
    if (ImGui::Button("+10")) { value_ += 10.0f; pulse_ = 1.0f; Clamp(); }
    ImGui::SameLine();
    if (ImGui::Button("-10")) { value_ -= 10.0f; Clamp(); }
    ImGui::SameLine();
    if (ImGui::Button("0 に戻す")) { Reset(); }
    ImGui::SameLine();
    if (ImGui::Button("捕獲テスト")) { value_ = params_.captureValue_; Clamp(); }

    bool changed = false;
    ImGui::SeparatorText("上がる");
    changed |= ImGui::DragFloat("視界に入っている間 /秒 (seenPerSec_)", &params_.seenPerSec_, 0.1f, 0.0f, 50.0f);
    changed |= ImGui::DragFloat("発見確定 (spottedAdd_)", &params_.spottedAdd_, 0.5f, 0.0f, 100.0f);
    changed |= ImGui::DragFloat("起きて通報 (wakeAdd_)", &params_.wakeAdd_, 0.5f, 0.0f, 100.0f);
    changed |= ImGui::Checkbox("騒音を数える (noiseEnabled_)", &params_.noiseEnabled_);
    changed |= ImGui::DragFloat("騒音 (noiseAdd_)", &params_.noiseAdd_, 0.5f, 0.0f, 100.0f);
    changed |= ImGui::DragFloat("騒音が届く距離 (noiseRadius_)", &params_.noiseRadius_, 0.1f, 0.0f, 30.0f);
    changed |= ImGui::DragFloat("騒音になる宝石の速さ (noiseSpeed_)", &params_.noiseSpeed_, 0.1f, 0.0f, 40.0f);
    changed |= ImGui::DragFloat("時間経過 /秒 (driftPerSec_)", &params_.driftPerSec_, 0.05f, 0.0f, 10.0f);
    ImGui::SeparatorText("下がる");
    changed |= ImGui::DragFloat("静かにしてから下がるまで 秒 (quietDelay_)", &params_.quietDelay_, 0.1f, 0.0f, 60.0f);
    changed |= ImGui::DragFloat("静かな時の減少 /秒 (quietDecayPerSec_)", &params_.quietDecayPerSec_, 0.1f, 0.0f, 20.0f);
    ImGui::SeparatorText("見返り・猶予");
    changed |= ImGui::DragFloat("遠くで起きたと見なす距離 (wakeFarDistance_)", &params_.wakeFarDistance_, 0.1f, 0.0f, 40.0f);
    changed |= ImGui::DragFloat("遠くで起きた時の通報 (wakeFarAdd_)", &params_.wakeFarAdd_, 0.5f, 0.0f, 100.0f);
    changed |= ImGui::DragFloat("復活直後の猶予 秒 (respawnGrace_)", &params_.respawnGrace_, 0.1f, 0.0f, 10.0f);
    ImGui::SeparatorText("捕獲");
    changed |= ImGui::DragFloat("捕獲になる値 (captureValue_)", &params_.captureValue_, 1.0f, 1.0f, 1000.0f);
    if (changed) {
        params_.captureValue_ = (std::max)(params_.captureValue_, 1.0f);
        ParameterManager* pm = ParameterManager::GetInstance();
        pm->SetValue(kGroup, "seenPerSec_", params_.seenPerSec_);
        pm->SetValue(kGroup, "spottedAdd_", params_.spottedAdd_);
        pm->SetValue(kGroup, "wakeAdd_", params_.wakeAdd_);
        pm->SetValue(kGroup, "noiseEnabled_", params_.noiseEnabled_);
        pm->SetValue(kGroup, "noiseAdd_", params_.noiseAdd_);
        pm->SetValue(kGroup, "noiseRadius_", params_.noiseRadius_);
        pm->SetValue(kGroup, "noiseSpeed_", params_.noiseSpeed_);
        pm->SetValue(kGroup, "driftPerSec_", params_.driftPerSec_);
        pm->SetValue(kGroup, "quietDelay_", params_.quietDelay_);
        pm->SetValue(kGroup, "quietDecayPerSec_", params_.quietDecayPerSec_);
        pm->SetValue(kGroup, "captureValue_", params_.captureValue_);
        pm->SetValue(kGroup, "wakeFarDistance_", params_.wakeFarDistance_);
        pm->SetValue(kGroup, "wakeFarAdd_", params_.wakeFarAdd_);
        pm->SetValue(kGroup, "respawnGrace_", params_.respawnGrace_);
    }
    if (ImGui::Button("パラメータを保存 (Alert)")) {
        SaveParams();
    }
    ImGui::TextDisabled("検算: 何もしないと %.0f 秒で捕獲。静かにすると実質 %.1f/秒で戻る",
                        params_.driftPerSec_ > 0.0f ? params_.captureValue_ / params_.driftPerSec_ : 0.0f,
                        params_.quietDecayPerSec_ - params_.driftPerSec_);
    if (!totals_.empty() || !counts_.empty()) {
        ImGui::SeparatorText("事象ごとの合計");
        for (const auto& [name, total] : totals_) {
            auto it = counts_.find(name);
            ImGui::Text("%s : %.1f (%d 回)", name.c_str(), total, it == counts_.end() ? 0 : it->second);
        }
        AlertRank r = ComputeRank();
        ImGui::Text("今クリアした場合の評価: %c  (発見 %d / 通報 %d / 騒音 %d / 最大 %.0f)", r.rank, r.spotted, r.reported, r.noises, r.peak);
    }
#endif
}
