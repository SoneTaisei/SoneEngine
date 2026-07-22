#include "ReplayIO.h"
#include "ReplayManager.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>

bool ReplayIO::SaveToFile(const ReplayData& data, const std::string& filename) {
    std::filesystem::create_directories("resources/json/saved_replays");
    std::string cleanName = filename;
    
    // ".json.mml" または ".json" の文字を除去する
    size_t jsonPos = cleanName.find(".json");
    if (jsonPos != std::string::npos) {
        cleanName.erase(jsonPos, 5);
    }
    
    // ".mml" が末尾になければ付加する
    if (cleanName.length() < 4 || cleanName.substr(cleanName.length() - 4) != ".mml") {
        cleanName += ".mml";
    }

    std::string filepath = "resources/json/saved_replays/" + cleanName;

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    // メタデータ
    ofs << "# MML Replay File" << std::endl;
    ofs << "[Metadata]" << std::endl;
    ofs << "Date=" << data.dateStr << std::endl;
    ofs << "StageFilename=" << data.stageFilename << std::endl;
    ofs << "TotalFrames=" << data.totalFrames << std::endl;
    ofs << "PlayerInitPos=" << data.playerInitPos.x << "," << data.playerInitPos.y << "," << data.playerInitPos.z << std::endl;
    ofs << "CameraInitPos=" << data.cameraInitPos.x << "," << data.cameraInitPos.y << "," << data.cameraInitPos.z << std::endl;
    ofs << std::endl;

    // 生マップデータセクション
    if (!data.mapDataStr.empty()) {
        ofs << "[MapData]" << std::endl;
        ofs << data.mapDataStr;
        if (data.mapDataStr.back() != '\n') ofs << std::endl;
        ofs << std::endl;
    }

    // MML トラックセクション
    ofs << "[MML]" << std::endl;
    ofs << "T0_LeftRight=" << data.mmlTracks[0] << std::endl;
    ofs << "T1_Jump=" << data.mmlTracks[1] << std::endl;
    ofs << "T2_Dash=" << data.mmlTracks[2] << std::endl;
    ofs << "T3_Cling=" << data.mmlTracks[3] << std::endl;
    ofs << "T4_UpDown=" << data.mmlTracks[4] << std::endl;
    ofs << std::endl;

    // 動的ブレ設定 (Jitters)
    if (!data.jitters.empty()) {
        ofs << "[Jitters]" << std::endl;
        for (const auto& j : data.jitters) {
            ofs << j.keyIdx << "," << j.startFrame << "," << j.endFrame << "," << j.maxJitter << std::endl;
        }
        ofs << std::endl;
    }
    
    // 適用済みマクロ (AppliedMacros)
    if (!data.appliedMacros.empty()) {
        ofs << "[AppliedMacros]" << std::endl;
        for (const auto& m : data.appliedMacros) {
            ofs << m.name << "," << m.startFrame << "," << m.duration << std::endl;
        }
        ofs << std::endl;
    }

    // 1フレームずつの状態データ (STR)
    ofs << "[STR]" << std::endl;
    for (int i = 0; i < data.totalFrames; ++i) {
        const auto& frame = data.frames[i];
        ofs << "F" << std::setw(4) << std::setfill('0') << i << "|"
            << frame.position.x << "," << frame.position.y << "," << frame.position.z << "|"
            << frame.cameraPosition.x << "," << frame.cameraPosition.y << "," << frame.cameraPosition.z << "|"
            << frame.keys << "|"
            << frame.color.x << "," << frame.color.y << "," << frame.color.z << "," << frame.color.w << "|"
            << frame.scale.x << "," << frame.scale.y << "," << frame.scale.z << "|"
            << frame.rotation.x << "," << frame.rotation.y << "," << frame.rotation.z << std::endl;
    }

    ofs.close();
    ReplayManager::GetInstance()->LoadSavedList(); // リストの更新
    return true;
}

bool ReplayIO::LoadFromFile(const std::string& filepath, ReplayData& outData) {
    std::string pathToOpen = filepath;
    if (!std::filesystem::exists(pathToOpen)) {
        std::string altPath = "resources/json/saved_replays/" + filepath;
        if (std::filesystem::exists(altPath)) {
            pathToOpen = altPath;
        }
    }

    std::ifstream ifs(pathToOpen);
    if (!ifs.is_open()) return false;

    outData.frames.clear();
    outData.filename = std::filesystem::path(pathToOpen).filename().string();
    
    std::string line;
    std::string currentSection = "";

    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[' && line[line.length() - 1] == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }

        std::stringstream ss(line);
        std::string key, value;

        if (currentSection == "MapData") {
            outData.mapDataStr += line + "\n";
        } else if (currentSection == "Metadata") {
            if (std::getline(ss, key, '=') && std::getline(ss, value)) {
                if (key == "Date") {
                    outData.dateStr = value;
                } else if (key == "StageFilename") {
                    outData.stageFilename = value;
                } else if (key == "TotalFrames") {
                    outData.totalFrames = std::stoi(value);
                } else if (key == "PlayerInitPos") {
                    std::stringstream vss(value);
                    std::string x, y, z;
                    if (std::getline(vss, x, ',') && std::getline(vss, y, ',') && std::getline(vss, z)) {
                        outData.playerInitPos = { std::stof(x), std::stof(y), std::stof(z) };
                    }
                } else if (key == "CameraInitPos") {
                    std::stringstream vss(value);
                    std::string x, y, z;
                    if (std::getline(vss, x, ',') && std::getline(vss, y, ',') && std::getline(vss, z)) {
                        outData.cameraInitPos = { std::stof(x), std::stof(y), std::stof(z) };
                    }
                }
            }
        } else if (currentSection == "MML") {
            if (std::getline(ss, key, '=') && std::getline(ss, value)) {
                if (key == "T0_LeftRight") outData.mmlTracks[0] = value;
                else if (key == "T1_Jump") outData.mmlTracks[1] = value;
                else if (key == "T2_Dash") outData.mmlTracks[2] = value;
                else if (key == "T3_Cling") outData.mmlTracks[3] = value;
                else if (key == "T4_UpDown") outData.mmlTracks[4] = value;
            }
        } else if (currentSection == "Jitters") {
            std::stringstream jss(line);
            std::string p1, p2, p3, p4;
            if (std::getline(jss, p1, ',') && std::getline(jss, p2, ',') && std::getline(jss, p3, ',') && std::getline(jss, p4)) {
                JitterSetting j;
                j.keyIdx = std::stoi(p1);
                j.startFrame = std::stoi(p2);
                j.endFrame = std::stoi(p3);
                j.maxJitter = std::stoi(p4);
                outData.jitters.push_back(j);
            }
        } else if (currentSection == "AppliedMacros") {
            std::stringstream amss(line);
            std::string p1, p2, p3;
            if (std::getline(amss, p1, ',') && std::getline(amss, p2, ',') && std::getline(amss, p3)) {
                AppliedMacro m;
                m.name = p1;
                m.startFrame = std::stoi(p2);
                m.duration = std::stoi(p3);
                outData.appliedMacros.push_back(m);
            }
        } else if (currentSection == "STR") {
            std::string parts[7];
            int partCount = 0;
            std::string part;
            while (std::getline(ss, part, '|') && partCount < 7) {
                parts[partCount++] = part;
            }
            
            if (partCount >= 3) {
                FrameData frame;
                std::string posStr = parts[1];
                std::string keysStr = parts[2];
                std::string camPosStr = "";
                std::string colorStr = "";
                std::string scaleStr = "";
                std::string rotStr = "";
                
                if (partCount == 4) {
                    camPosStr = parts[2];
                    keysStr = parts[3];
                } else if (partCount >= 5) {
                    camPosStr = parts[2];
                    keysStr = parts[3];
                    colorStr = parts[4];
                    if (partCount >= 7) {
                        scaleStr = parts[5];
                        rotStr = parts[6];
                    }
                }
                
                // 座標パース
                std::stringstream pss(posStr);
                std::string x, y, z;
                if (std::getline(pss, x, ',') && std::getline(pss, y, ',') && std::getline(pss, z)) {
                    frame.position = { std::stof(x), std::stof(y), std::stof(z) };
                }
                
                // カメラ座標パース
                if (!camPosStr.empty()) {
                    std::stringstream cpss(camPosStr);
                    if (std::getline(cpss, x, ',') && std::getline(cpss, y, ',') && std::getline(cpss, z)) {
                        frame.cameraPosition = { std::stof(x), std::stof(y), std::stof(z) };
                    }
                } else {
                    frame.cameraPosition = outData.cameraInitPos;
                }

                // キー状態コピー
                for (int i = 0; i < 7 && i < static_cast<int>(keysStr.length()); ++i) {
                    frame.keys[i] = keysStr[i];
                }
                for (int i = static_cast<int>(keysStr.length()); i < 7; ++i) {
                    frame.keys[i] = '-';
                }
                frame.keys[7] = '\0';
                
                // カラーのパース
                if (!colorStr.empty()) {
                    std::stringstream css(colorStr);
                    std::string r, g, b, a;
                    if (std::getline(css, r, ',') && std::getline(css, g, ',') && std::getline(css, b, ',') && std::getline(css, a)) {
                        frame.color = { std::stof(r), std::stof(g), std::stof(b), std::stof(a) };
                    }
                }

                // スケールのパース
                if (!scaleStr.empty()) {
                    std::stringstream sss(scaleStr);
                    std::string sx, sy, sz;
                    if (std::getline(sss, sx, ',') && std::getline(sss, sy, ',') && std::getline(sss, sz)) {
                        frame.scale = { std::stof(sx), std::stof(sy), std::stof(sz) };
                    }
                }

                // 回転のパース
                if (!rotStr.empty()) {
                    std::stringstream rss(rotStr);
                    std::string rx, ry, rz;
                    if (std::getline(rss, rx, ',') && std::getline(rss, ry, ',') && std::getline(rss, rz)) {
                        frame.rotation = { std::stof(rx), std::stof(ry), std::stof(rz) };
                    }
                }
                
                outData.frames.push_back(frame);
            }
        }
    }

    ifs.close();

    if (outData.frames.empty() && outData.totalFrames > 0) {
        ReplayManager::GetInstance()->RebuildFramesFromMml(outData);
    } else {
        outData.totalFrames = static_cast<int>(outData.frames.size());
    }

    return true;
}

std::vector<std::string> ReplayIO::GetSavedFileList() {
    std::vector<std::string> list;
    std::filesystem::create_directories("resources/json/saved_replays");
    for (const auto& entry : std::filesystem::directory_iterator("resources/json/saved_replays")) {
        if (entry.is_regular_file() && entry.path().extension() == ".mml") {
            list.push_back(entry.path().filename().string());
        }
    }
    return list;
}

void ReplayIO::DeleteSavedFile(const std::string& filepath) {
    std::string fullpath = "resources/json/saved_replays/" + filepath;
    if (std::filesystem::exists(fullpath)) {
        std::filesystem::remove(fullpath);
    }
    ReplayManager::GetInstance()->LoadSavedList();
}

std::vector<ReplayMacro> ReplayIO::LoadMacros() {
    std::vector<ReplayMacro> macros;
    std::ifstream ifs("json/replay_macros.txt");
    if (!ifs.is_open()) return macros;

    std::string line;
    ReplayMacro* currentMacro = nullptr;

    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[' && line[line.length() - 1] == ']') {
            macros.push_back(ReplayMacro());
            currentMacro = &macros.back();
            currentMacro->name = line.substr(1, line.length() - 2);
            continue;
        }

        if (currentMacro) {
            std::stringstream ss(line);
            std::string durStr, keysStr;
            if (std::getline(ss, durStr, ',') && std::getline(ss, keysStr)) {
                MacroBlock b;
                b.duration = std::stoi(durStr);
                strncpy_s(b.keys, keysStr.c_str(), sizeof(b.keys));
                currentMacro->blocks.push_back(b);
            }
        }
    }
    return macros;
}

void ReplayIO::SaveMacros(const std::vector<ReplayMacro>& macros) {
    std::filesystem::create_directories("json");
    std::ofstream ofs("json/replay_macros.txt");
    if (!ofs.is_open()) return;

    for (const auto& macro : macros) {
        ofs << "[" << macro.name << "]" << std::endl;
        for (const auto& block : macro.blocks) {
            ofs << block.duration << "," << block.keys << std::endl;
        }
        ofs << std::endl;
    }
}
