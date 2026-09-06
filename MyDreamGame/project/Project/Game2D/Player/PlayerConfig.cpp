#include "PlayerConfig.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

void PlayerConfig::Save(const PlayerParams& params, const std::string& filepath) {
    try {
        std::filesystem::path path(filepath);
        if (path.has_parent_path() && !std::filesystem::exists(path.parent_path())) {
            std::filesystem::create_directories(path.parent_path());
        }
        
        nlohmann::json j;
        j["moveSpeed_"] = params.moveSpeed_;
        j["jumpPower_"] = params.jumpPower_;
        j["gravity_"] = params.gravity_;
        j["maxFallSpeed_"] = params.maxFallSpeed_;
        j["halfWidth_"] = params.halfWidth_;
        j["halfHeight_"] = params.halfHeight_;
        j["colorNormal_"] = { params.colorNormal_.x, params.colorNormal_.y, params.colorNormal_.z, params.colorNormal_.w };
        j["deathDuration_"] = params.deathDuration_;
        j["respawnDuration_"] = params.respawnDuration_;
        j["goalWaitTime_"] = params.goalWaitTime_;
        j["chainJumpPenalty_"] = params.chainJumpPenalty_;
        j["modelScale_"] = params.modelScale_;
        
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
            std::cout << "Player parameters saved to " << filepath << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to save player parameters: " << e.what() << std::endl;
    }
}

void PlayerConfig::Load(PlayerParams& params, const std::string& filepath) {
    try {
        if (!std::filesystem::exists(filepath)) return;
        
        std::ifstream file(filepath);
        if (!file.is_open()) return;
        
        nlohmann::json j;
        file >> j;
        file.close();
        
        if (j.contains("moveSpeed_")) params.moveSpeed_ = j["moveSpeed_"];
        if (j.contains("jumpPower_")) params.jumpPower_ = j["jumpPower_"];
        if (j.contains("gravity_")) params.gravity_ = j["gravity_"];
        if (j.contains("maxFallSpeed_")) params.maxFallSpeed_ = j["maxFallSpeed_"];
        if (j.contains("halfWidth_")) params.halfWidth_ = j["halfWidth_"];
        if (j.contains("halfHeight_")) params.halfHeight_ = j["halfHeight_"];
        
        if (j.contains("colorNormal_")) {
            params.colorNormal_.x = j["colorNormal_"][0];
            params.colorNormal_.y = j["colorNormal_"][1];
            params.colorNormal_.z = j["colorNormal_"][2];
            params.colorNormal_.w = j["colorNormal_"][3];
        }
        if (j.contains("deathDuration_")) params.deathDuration_ = j["deathDuration_"];
        if (j.contains("respawnDuration_")) params.respawnDuration_ = j["respawnDuration_"];
        if (j.contains("goalWaitTime_")) params.goalWaitTime_ = j["goalWaitTime_"];
        if (j.contains("chainJumpPenalty_")) params.chainJumpPenalty_ = j["chainJumpPenalty_"];
        if (j.contains("modelScale_")) params.modelScale_ = j["modelScale_"];
        
        std::cout << "Player parameters loaded from " << filepath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load player parameters: " << e.what() << std::endl;
    }
}
