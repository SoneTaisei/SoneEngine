#include "CollectibleTracker.h"
#include "Game2D/MapChip2D.h"
#include "Game2D/Blocks/CollectibleBlock.h"
#include "Core/Utility/UtilityFunctions.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {
    constexpr const char* kSavePath = "resources/json/local/collectibles.json";
    constexpr float kPulseDecay = 3.0f; // 跳ねが消えるまで約 0.33 秒

    std::string StageKeyOf(const std::string& mapFilePath) {
        std::filesystem::path p(mapFilePath);
        std::string stem = p.stem().string();
        return stem.empty() ? mapFilePath : stem;
    }
}

CollectibleTracker& CollectibleTracker::Get() {
    static CollectibleTracker instance;
    return instance;
}

void CollectibleTracker::BeginStage(const std::string& mapFilePath) {
    stageKey_ = StageKeyOf(mapFilePath);
    attempt_.clear();
    last_ = { -1, -1 };
    pulse_ = 0.0f;
    Load();
}

bool CollectibleTracker::WasCollectedBefore(int chipX, int chipY) const {
    return saved_.count({ chipX, chipY }) > 0;
}

void CollectibleTracker::OnCollected(int chipX, int chipY) {
    attempt_.insert({ chipX, chipY });
    last_ = { chipX, chipY };
    pulse_ = 1.0f;
}

void CollectibleTracker::CommitStageClear() {
    if (attempt_.empty()) return;
    size_t before = saved_.size();
    saved_.insert(attempt_.begin(), attempt_.end());
    if (saved_.size() != before) {
        Save();
        Log("CollectibleTracker: saved " + std::to_string(saved_.size()) + " gem(s) for stage " + stageKey_ + "\n");
    }
}

CollectibleTracker::Summary CollectibleTracker::Summarize(const MapChip2D* map) const {
    Summary s;
    if (!map) return s;
    const int w = map->GetWidth();
    const int h = map->GetHeight();
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            const auto* gem = dynamic_cast<const CollectibleBlock*>(map->GetBlock(x, y));
            if (!gem || gem->GetChipX() != x || gem->GetChipY() != y) continue; // 同じブロックを二度数えない
            Entry e;
            e.x = x;
            e.y = y;
            e.collectedNow = gem->IsCollectedNow();
            e.collectedBefore = gem->WasCollectedBefore();
            s.entries.push_back(e);
        }
    }
    s.total = static_cast<int>(s.entries.size());
    for (const auto& e : s.entries) {
        if (e.collectedNow) ++s.collectedNow;
        if (e.collectedBefore) ++s.collectedBefore;
    }
    return s;
}

void CollectibleTracker::TickPulse(float dt) {
    pulse_ = (std::max)(0.0f, pulse_ - dt * kPulseDecay);
}

void CollectibleTracker::Load() {
    saved_.clear();
    all_ = nlohmann::json::object();
    std::ifstream ifs(kSavePath);
    if (!ifs) return;
    try {
        ifs >> all_;
    } catch (...) {
        all_ = nlohmann::json::object();
        return;
    }
    if (!all_.is_object() || !all_.contains("stages") || !all_["stages"].is_object()) return;
    const auto& stages = all_["stages"];
    if (!stages.contains(stageKey_) || !stages[stageKey_].is_array()) return;
    for (const auto& item : stages[stageKey_]) {
        if (item.is_array() && item.size() >= 2 && item[0].is_number_integer() && item[1].is_number_integer()) {
            saved_.insert({ item[0].get<int>(), item[1].get<int>() });
        }
    }
}

void CollectibleTracker::Save() const {
    nlohmann::json out = all_.is_object() ? all_ : nlohmann::json::object();
    if (!out.contains("stages") || !out["stages"].is_object()) {
        out["stages"] = nlohmann::json::object();
    }
    nlohmann::json list = nlohmann::json::array();
    for (const auto& p : saved_) {
        list.push_back({ p.first, p.second });
    }
    out["stages"][stageKey_] = list;
    std::filesystem::create_directories(std::filesystem::path(kSavePath).parent_path());
    std::ofstream ofs(kSavePath);
    if (!ofs) return;
    ofs << out.dump(4);
}
