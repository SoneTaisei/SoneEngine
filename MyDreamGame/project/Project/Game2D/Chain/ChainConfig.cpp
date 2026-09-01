#include "ChainConfig.h"
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
        j["nodeCount_"] = params.nodeCount_;
        j["totalLength_"] = params.totalLength_;
        j["gravity_"] = params.gravity_;
        j["damping_"] = params.damping_;
        j["iterations_"] = params.iterations_;
        j["subSteps_"] = params.subSteps_;
        j["nodeRadius_"] = params.nodeRadius_;
        j["friction_"] = params.friction_;
        j["playerVelInfluence_"] = params.playerVelInfluence_;
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

        if (j.contains("nodeCount_")) params.nodeCount_ = j["nodeCount_"];
        if (j.contains("totalLength_")) params.totalLength_ = j["totalLength_"];
        if (j.contains("gravity_")) params.gravity_ = j["gravity_"];
        if (j.contains("damping_")) params.damping_ = j["damping_"];
        if (j.contains("iterations_")) params.iterations_ = j["iterations_"];
        if (j.contains("subSteps_")) params.subSteps_ = j["subSteps_"];
        if (j.contains("nodeRadius_")) params.nodeRadius_ = j["nodeRadius_"];
        if (j.contains("friction_")) params.friction_ = j["friction_"];
        if (j.contains("playerVelInfluence_")) params.playerVelInfluence_ = j["playerVelInfluence_"];
        if (j.contains("linkThickness_")) params.linkThickness_ = j["linkThickness_"];
        if (j.contains("linkOverlap_")) params.linkOverlap_ = j["linkOverlap_"];

        std::cout << "Chain parameters loaded from " << filepath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load chain parameters: " << e.what() << std::endl;
    }
}
