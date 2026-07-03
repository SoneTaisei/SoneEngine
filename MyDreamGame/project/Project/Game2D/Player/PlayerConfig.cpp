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
        
        j["dashDuration_"] = params.dashDuration_;
        j["dashSpeed_"] = params.dashSpeed_;
        j["dashEndUpwardVelocity_"] = params.dashEndUpwardVelocity_;
        
        j["wallJumpDuration_"] = params.wallJumpDuration_;
        j["wallJumpPower_"] = { params.wallJumpPower_.x, params.wallJumpPower_.y };
        j["wallJumpDirLockDuration_"] = params.wallJumpDirLockDuration_;
        j["wallSlideSpeed_"] = params.wallSlideSpeed_;
        j["wallClimbSpeed_"] = params.wallClimbSpeed_;
        j["wallClingReleaseDuration_"] = params.wallClingReleaseDuration_;
        
        j["halfWidth_"] = params.halfWidth_;
        j["halfHeight_"] = params.halfHeight_;
        
        j["colorNormal_"] = { params.colorNormal_.x, params.colorNormal_.y, params.colorNormal_.z, params.colorNormal_.w };
        j["colorDashed_"] = { params.colorDashed_.x, params.colorDashed_.y, params.colorDashed_.z, params.colorDashed_.w };
        
        j["deathDuration_"] = params.deathDuration_;
        j["respawnDuration_"] = params.respawnDuration_;
        j["goalWaitTime_"] = params.goalWaitTime_;
        j["runDustInterval_"] = params.runDustInterval_;
        
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
        
        if (j.contains("dashDuration_")) params.dashDuration_ = j["dashDuration_"];
        if (j.contains("dashSpeed_")) params.dashSpeed_ = j["dashSpeed_"];
        if (j.contains("dashEndUpwardVelocity_")) params.dashEndUpwardVelocity_ = j["dashEndUpwardVelocity_"];
        
        if (j.contains("wallJumpDuration_")) params.wallJumpDuration_ = j["wallJumpDuration_"];
        if (j.contains("wallJumpPower_")) {
            params.wallJumpPower_.x = j["wallJumpPower_"][0];
            params.wallJumpPower_.y = j["wallJumpPower_"][1];
        }
        if (j.contains("wallJumpDirLockDuration_")) params.wallJumpDirLockDuration_ = j["wallJumpDirLockDuration_"];
        if (j.contains("wallSlideSpeed_")) params.wallSlideSpeed_ = j["wallSlideSpeed_"];
        if (j.contains("wallClimbSpeed_")) params.wallClimbSpeed_ = j["wallClimbSpeed_"];
        if (j.contains("wallClingReleaseDuration_")) params.wallClingReleaseDuration_ = j["wallClingReleaseDuration_"];
        
        if (j.contains("halfWidth_")) params.halfWidth_ = j["halfWidth_"];
        if (j.contains("halfHeight_")) params.halfHeight_ = j["halfHeight_"];
        
        if (j.contains("colorNormal_")) {
            params.colorNormal_.x = j["colorNormal_"][0];
            params.colorNormal_.y = j["colorNormal_"][1];
            params.colorNormal_.z = j["colorNormal_"][2];
            params.colorNormal_.w = j["colorNormal_"][3];
        }
        if (j.contains("colorDashed_")) {
            params.colorDashed_.x = j["colorDashed_"][0];
            params.colorDashed_.y = j["colorDashed_"][1];
            params.colorDashed_.z = j["colorDashed_"][2];
            params.colorDashed_.w = j["colorDashed_"][3];
        }
        
        if (j.contains("deathDuration_")) params.deathDuration_ = j["deathDuration_"];
        if (j.contains("respawnDuration_")) params.respawnDuration_ = j["respawnDuration_"];
        if (j.contains("goalWaitTime_")) params.goalWaitTime_ = j["goalWaitTime_"];
        if (j.contains("runDustInterval_")) params.runDustInterval_ = j["runDustInterval_"];
        
        std::cout << "Player parameters loaded from " << filepath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load player parameters: " << e.what() << std::endl;
    }
}
