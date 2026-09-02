#include "ChainConfig.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

const std::string ChainConfig::kDefaultFilePath = "resources/json/shared/Chain/chain_parameters.json";

void ChainConfig::Save(const ChainParams& params, const std::string& filepath) {
    try {
        std::filesystem::path path(filepath);
        if (path.has_parent_path() && !std::filesystem::exists(path.parent_path())) {
            std::filesystem::create_directories(path.parent_path());
        }

        nlohmann::json j;
        j["initialUnits_"] = params.initialUnits_;
        j["unitLength_"] = params.unitLength_;
        j["nodesPerUnit_"] = params.nodesPerUnit_;
        j["maxUnits_"] = params.maxUnits_;
        j["minUnits_"] = params.minUnits_;
        j["unitsPerAction_"] = params.unitsPerAction_;
        j["pickupRadius_"] = params.pickupRadius_;
        j["payoutSpeed_"] = params.payoutSpeed_;
        j["gravity_"] = params.gravity_;
        j["damping_"] = params.damping_;
        j["iterations_"] = params.iterations_;
        j["subSteps_"] = params.subSteps_;
        j["nodeRadius_"] = params.nodeRadius_;
        j["friction_"] = params.friction_;
        j["playerVelInfluence_"] = params.playerVelInfluence_;
        j["rootCollisionSkip_"] = params.rootCollisionSkip_;
        j["treasureMass_"] = params.treasureMass_;
        j["treasureRadius_"] = params.treasureRadius_;
        j["treasureFriction_"] = params.treasureFriction_;
        j["treasureIgnorePlayer_"] = params.treasureIgnorePlayer_;
        j["heldChainPlayerCollision_"] = params.heldChainPlayerCollision_;
        j["treasureModelDir_"] = params.treasureModelDir_;
        j["treasureModelFile_"] = params.treasureModelFile_;
        j["treasureScale_"] = params.treasureScale_;
        j["spinRadiusMax_"] = params.spinRadiusMax_;
        j["spinRadiusRatio_"] = params.spinRadiusRatio_;
        j["swingStrength_"] = params.swingStrength_;
        j["swingDamping_"] = params.swingDamping_;
        j["chainMassPerUnit_"] = params.chainMassPerUnit_;
        j["weightThrowScale_"] = params.weightThrowScale_;
        j["pullDelay_"] = params.pullDelay_;
        j["pullTransfer_"] = params.pullTransfer_;
        j["launchMaxJumpRatio_"] = params.launchMaxJumpRatio_;
        j["launchMinUpward_"] = params.launchMinUpward_;
        j["spinMoveFactor_"] = params.spinMoveFactor_;
        j["spinCooldown_"] = params.spinCooldown_;
        j["linkThickness_"] = params.linkThickness_;
        j["linkOverlap_"] = params.linkOverlap_;

        std::ofstream file(filepath);
        if (file.is_open()) {
            file << j.dump(4);
            file.close();
            std::cout << "Chain parameters saved to " << filepath << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to save chain parameters: " << e.what() << std::endl;
    }
}

void ChainConfig::Load(ChainParams& params, const std::string& filepath) {
    try {
        if (!std::filesystem::exists(filepath)) return;

        std::ifstream file(filepath);
        if (!file.is_open()) return;

        nlohmann::json j;
        file >> j;
        file.close();

        // 旧形式のキー(nodeCount_/totalLength_/omegaMax_/launchSpeedScale_/tautRatio_ 等)は無視される
        if (j.contains("initialUnits_")) params.initialUnits_ = j["initialUnits_"];
        if (j.contains("unitLength_")) params.unitLength_ = j["unitLength_"];
        if (j.contains("nodesPerUnit_")) params.nodesPerUnit_ = j["nodesPerUnit_"];
        if (j.contains("maxUnits_")) params.maxUnits_ = j["maxUnits_"];
        if (j.contains("minUnits_")) params.minUnits_ = j["minUnits_"];
        if (j.contains("unitsPerAction_")) params.unitsPerAction_ = j["unitsPerAction_"];
        if (j.contains("pickupRadius_")) params.pickupRadius_ = j["pickupRadius_"];
        if (j.contains("payoutSpeed_")) params.payoutSpeed_ = j["payoutSpeed_"];
        if (j.contains("gravity_")) params.gravity_ = j["gravity_"];
        if (j.contains("damping_")) params.damping_ = j["damping_"];
        if (j.contains("iterations_")) params.iterations_ = j["iterations_"];
        if (j.contains("subSteps_")) params.subSteps_ = j["subSteps_"];
        if (j.contains("nodeRadius_")) params.nodeRadius_ = j["nodeRadius_"];
        if (j.contains("friction_")) params.friction_ = j["friction_"];
        if (j.contains("playerVelInfluence_")) params.playerVelInfluence_ = j["playerVelInfluence_"];
        if (j.contains("rootCollisionSkip_")) params.rootCollisionSkip_ = j["rootCollisionSkip_"];
        if (j.contains("treasureMass_")) params.treasureMass_ = j["treasureMass_"];
        if (j.contains("treasureRadius_")) params.treasureRadius_ = j["treasureRadius_"];
        if (j.contains("treasureFriction_")) params.treasureFriction_ = j["treasureFriction_"];
        if (j.contains("treasureIgnorePlayer_")) params.treasureIgnorePlayer_ = j["treasureIgnorePlayer_"];
        if (j.contains("heldChainPlayerCollision_")) params.heldChainPlayerCollision_ = j["heldChainPlayerCollision_"];
        if (j.contains("treasureModelDir_")) params.treasureModelDir_ = j["treasureModelDir_"].get<std::string>();
        if (j.contains("treasureModelFile_")) params.treasureModelFile_ = j["treasureModelFile_"].get<std::string>();
        if (j.contains("treasureScale_")) params.treasureScale_ = j["treasureScale_"];
        if (j.contains("spinRadiusMax_")) params.spinRadiusMax_ = j["spinRadiusMax_"];
        if (j.contains("spinRadiusRatio_")) params.spinRadiusRatio_ = j["spinRadiusRatio_"];
        if (j.contains("swingStrength_")) params.swingStrength_ = j["swingStrength_"];
        if (j.contains("swingDamping_")) params.swingDamping_ = j["swingDamping_"];
        if (j.contains("chainMassPerUnit_")) params.chainMassPerUnit_ = j["chainMassPerUnit_"];
        if (j.contains("weightThrowScale_")) params.weightThrowScale_ = j["weightThrowScale_"];
        if (j.contains("pullDelay_")) params.pullDelay_ = j["pullDelay_"];
        if (j.contains("pullTransfer_")) params.pullTransfer_ = j["pullTransfer_"];
        if (j.contains("launchMaxJumpRatio_")) params.launchMaxJumpRatio_ = j["launchMaxJumpRatio_"];
        if (j.contains("launchMinUpward_")) params.launchMinUpward_ = j["launchMinUpward_"];
        if (j.contains("spinMoveFactor_")) params.spinMoveFactor_ = j["spinMoveFactor_"];
        if (j.contains("spinCooldown_")) params.spinCooldown_ = j["spinCooldown_"];
        if (j.contains("linkThickness_")) params.linkThickness_ = j["linkThickness_"];
        if (j.contains("linkOverlap_")) params.linkOverlap_ = j["linkOverlap_"];

        // 値の整合性を保証する（手編集されたJSONでも min>max 等で std::clamp が未定義動作にならないように。
        // nodesPerUnit_ が 1 だと切り離した鎖が1ノードになり、見えない拾得点が浮くため 2 以上に制限）
        params.nodesPerUnit_ = (std::max)(2, params.nodesPerUnit_);
        params.maxUnits_ = (std::max)(1, params.maxUnits_);
        params.minUnits_ = std::clamp(params.minUnits_, 1, params.maxUnits_);
        params.unitsPerAction_ = (std::max)(1, params.unitsPerAction_);
        params.initialUnits_ = (std::max)(1, params.initialUnits_);
        params.unitLength_ = (std::max)(0.1f, params.unitLength_);
        params.payoutSpeed_ = (std::max)(0.1f, params.payoutSpeed_);
        params.treasureMass_ = (std::max)(0.1f, params.treasureMass_);
        params.treasureRadius_ = (std::max)(0.05f, params.treasureRadius_);
        params.treasureFriction_ = std::clamp(params.treasureFriction_, 0.0f, 1.0f);
        params.treasureScale_ = (std::max)(0.01f, params.treasureScale_);
        if (params.treasureModelDir_.empty()) params.treasureModelDir_ = "resources/Object/Original/sphere";
        if (params.treasureModelFile_.empty()) params.treasureModelFile_ = "sphere.obj";
        params.spinRadiusMax_ = (std::max)(0.3f, params.spinRadiusMax_);
        params.spinRadiusRatio_ = std::clamp(params.spinRadiusRatio_, 0.3f, 1.0f);
        params.swingStrength_ = (std::max)(0.0f, params.swingStrength_);
        params.swingDamping_ = (std::max)(0.0f, params.swingDamping_);
        params.chainMassPerUnit_ = (std::max)(0.0f, params.chainMassPerUnit_);
        params.weightThrowScale_ = (std::max)(0.0f, params.weightThrowScale_);
        params.pullDelay_ = std::clamp(params.pullDelay_, 0.0f, 1.0f);
        params.pullTransfer_ = (std::max)(0.0f, params.pullTransfer_);
        // 上限倍率 1.7 × ジャンプ初速17.5 ≒ 30 u/s。プレイヤーの当たり判定は掃引しないため、これ以上は1チップ壁をすり抜け得る
        params.launchMaxJumpRatio_ = std::clamp(params.launchMaxJumpRatio_, 0.1f, 1.7f);
        params.launchMinUpward_ = std::clamp(params.launchMinUpward_, 0.0f, 1.0f);
        params.spinMoveFactor_ = std::clamp(params.spinMoveFactor_, 0.0f, 1.0f);
        params.spinCooldown_ = (std::max)(0.0f, params.spinCooldown_);

        std::cout << "Chain parameters loaded from " << filepath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load chain parameters: " << e.what() << std::endl;
    }
}
