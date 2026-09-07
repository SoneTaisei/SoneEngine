#include "ReplayIO.h"
#include "ReplayManager.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace {
// 動的オブジェクトの状態が実質同じかどうか（差分保存の判定用）
bool IsSameObjectState(const ReplayObjectState& a, const ReplayObjectState& b) {
    const float kEpsilon = 0.0001f;
    auto same = [&](float x, float y) { return std::fabs(x - y) <= kEpsilon; };

    if (a.destroyed != b.destroyed) return false;
    if (!same(a.position.x, b.position.x) || !same(a.position.y, b.position.y) || !same(a.position.z, b.position.z)) return false;
    if (!same(a.rotation.x, b.rotation.x) || !same(a.rotation.y, b.rotation.y) || !same(a.rotation.z, b.rotation.z)) return false;
    if (!same(a.scale.x, b.scale.x) || !same(a.scale.y, b.scale.y) || !same(a.scale.z, b.scale.z)) return false;
    if (!same(a.color.x, b.color.x) || !same(a.color.y, b.color.y) || !same(a.color.z, b.color.z) || !same(a.color.w, b.color.w)) return false;
    if (a.custom.size() != b.custom.size()) return false;
    for (size_t i = 0; i < a.custom.size(); ++i) {
        if (!same(a.custom[i], b.custom[i])) return false;
    }
    return true;
}
} // namespace

bool ReplayIO::SaveToFile(const ReplayData& data, const std::string& filename) {
    std::filesystem::create_directories("resources/json/local/saved_replays");
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

    std::string filepath = "resources/json/local/saved_replays/" + cleanName;

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
            << frame.rotation.x << "," << frame.rotation.y << "," << frame.rotation.z << "|"
            << std::setprecision(9) << frame.time << std::setprecision(6) << std::endl;
    }

    // 動的オブジェクト（動く床・扉・スイッチ等）の状態
    // 1行 = 「フレーム番号|ID,破壊フラグ,pos,rot,scale,color[,固有状態...]」
    // 直前フレームから変化がないオブジェクトは書き出さない（読み込み時に前フレームから引き継ぐ）
    if (!data.objectFrames.empty()) {
        ofs << std::endl;
        ofs << "[Objects]" << std::endl;
        std::vector<ReplayObjectState> prevStates;
        for (size_t f = 0; f < data.objectFrames.size(); ++f) {
            for (const auto& state : data.objectFrames[f].states) {
                auto prevIt = std::find_if(prevStates.begin(), prevStates.end(),
                    [&state](const ReplayObjectState& p) { return p.id == state.id; });
                if (prevIt != prevStates.end() && IsSameObjectState(*prevIt, state)) continue;

                ofs << f << "|" << state.id << "," << (state.destroyed ? 1 : 0) << ","
                    << state.position.x << "," << state.position.y << "," << state.position.z << ","
                    << state.rotation.x << "," << state.rotation.y << "," << state.rotation.z << ","
                    << state.scale.x << "," << state.scale.y << "," << state.scale.z << ","
                    << state.color.x << "," << state.color.y << "," << state.color.z << "," << state.color.w;
                for (float value : state.custom) {
                    ofs << "," << value;
                }
                ofs << std::endl;

                if (prevIt != prevStates.end()) {
                    *prevIt = state;
                } else {
                    prevStates.push_back(state);
                }
            }
        }
    }

    ofs.close();
    ReplayManager::GetInstance()->LoadSavedList(); // リストの更新
    return true;
}

bool ReplayIO::LoadFromFile(const std::string& filepath, ReplayData& outData) {
    std::string pathToOpen = filepath;
    if (!std::filesystem::exists(pathToOpen)) {
        std::string altPath = "resources/json/local/saved_replays/" + filepath;
        if (std::filesystem::exists(altPath)) {
            pathToOpen = altPath;
        }
    }

    std::ifstream ifs(pathToOpen);
    if (!ifs.is_open()) return false;

    outData.frames.clear();
    outData.objectFrames.clear();
    outData.filename = std::filesystem::path(pathToOpen).filename().string();

    // [Objects] は差分保存なので、いったん「フレーム番号 + 状態」の並びで読み込む
    struct ObjectRecord {
        int frame = 0;
        ReplayObjectState state;
    };
    std::vector<ObjectRecord> objectRecords;

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
        } else if (currentSection == "Objects") {
            // 「フレーム番号|ID,破壊フラグ,pos,rot,scale,color[,固有状態...]」
            std::string frameStr, body;
            if (!std::getline(ss, frameStr, '|') || !std::getline(ss, body)) continue;

            std::vector<float> values;
            std::stringstream bss(body);
            std::string token;
            while (std::getline(bss, token, ',')) {
                if (token.empty()) continue;
                values.push_back(std::stof(token));
            }
            if (values.size() < 15) continue; // id,破壊フラグ,pos(3),rot(3),scale(3),color(4)

            ReplayObjectState state;
            state.id = static_cast<uint64_t>(std::stoull(body.substr(0, body.find(','))));
            state.destroyed = (values[1] != 0.0f);
            state.position = { values[2], values[3], values[4] };
            state.rotation = { values[5], values[6], values[7] };
            state.scale = { values[8], values[9], values[10] };
            state.color = { values[11], values[12], values[13], values[14] };
            state.custom.assign(values.begin() + 15, values.end());

            objectRecords.push_back({ std::stoi(frameStr), std::move(state) });
        } else if (currentSection == "STR") {
            std::string parts[8];
            int partCount = 0;
            std::string part;
            while (std::getline(ss, part, '|') && partCount < 8) {
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
                std::string timeStr = "";
                
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
                    if (partCount >= 8) {
                        timeStr = parts[7];
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

                // ゲーム内時刻（旧フォーマットには無いので、その場合は 1/60 刻みで補完する）
                if (!timeStr.empty()) {
                    frame.time = std::stof(timeStr);
                } else {
                    frame.time = static_cast<float>(outData.frames.size()) / 60.0f;
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

    // 差分で保存された動的オブジェクトの状態を、全フレーム分に展開する
    if (!objectRecords.empty() && outData.totalFrames > 0) {
        outData.objectFrames.assign(outData.totalFrames, ReplayObjectFrame{});

        std::vector<ReplayObjectState> activeStates; // 現在有効な状態（IDごとに最新のもの）
        size_t recordIdx = 0;
        for (int f = 0; f < outData.totalFrames; ++f) {
            while (recordIdx < objectRecords.size() && objectRecords[recordIdx].frame <= f) {
                const ReplayObjectState& state = objectRecords[recordIdx].state;
                auto it = std::find_if(activeStates.begin(), activeStates.end(),
                    [&state](const ReplayObjectState& s) { return s.id == state.id; });
                if (it != activeStates.end()) {
                    *it = state;
                } else {
                    activeStates.push_back(state);
                }
                ++recordIdx;
            }
            outData.objectFrames[f].states = activeStates;
        }
    }

    return true;
}

std::vector<std::string> ReplayIO::GetSavedFileList() {
    std::vector<std::string> list;
    std::filesystem::create_directories("resources/json/local/saved_replays");
    for (const auto& entry : std::filesystem::directory_iterator("resources/json/local/saved_replays")) {
        if (entry.is_regular_file() && entry.path().extension() == ".mml") {
            list.push_back(entry.path().filename().string());
        }
    }
    return list;
}

void ReplayIO::DeleteSavedFile(const std::string& filepath) {
    std::string fullpath = "resources/json/local/saved_replays/" + filepath;
    if (std::filesystem::exists(fullpath)) {
        std::filesystem::remove(fullpath);
    }
    ReplayManager::GetInstance()->LoadSavedList();
}

std::vector<ReplayMacro> ReplayIO::LoadMacros() {
    std::vector<ReplayMacro> macros;
    std::ifstream ifs("resources/json/local/replay_macros.txt");
    if (!ifs.is_open()) {
        // フォールバック: 旧パスにあれば読み込む
        ifs.open("resources/json/replay_macros.txt");
        if (!ifs.is_open()) {
            ifs.open("json/replay_macros.txt");
            if (!ifs.is_open()) return macros;
        }
    }

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
    std::filesystem::create_directories("resources/json/local");
    std::ofstream ofs("resources/json/local/replay_macros.txt");
    if (!ofs.is_open()) return;

    for (const auto& macro : macros) {
        ofs << "[" << macro.name << "]" << std::endl;
        for (const auto& block : macro.blocks) {
            ofs << block.duration << "," << block.keys << std::endl;
        }
        ofs << std::endl;
    }
}
