#ifdef USE_IMGUI
#include "MapEditorContext.h"
#include "Scene/SceneManager.h"
#include "Scene/IScene.h"
#include "Game2D/Player/Player2D.h"
#include "Editor/Replay/ReplayManager.h"
#include <filesystem>
#include <algorithm>

namespace {
    class MapEditCommand : public MapEditorContext::IMapCommand {
        MapChip2D* mapChip_;
        MapEditorContext::MapState oldState_;
        MapEditorContext::MapState newState_;
    public:
        MapEditCommand(MapChip2D* chip, const MapEditorContext::MapState& oldS, const MapEditorContext::MapState& newS)
            : mapChip_(chip), oldState_(oldS), newState_(newS) {}
        void Undo() override {
            if (!mapChip_) return;
            mapChip_->Resize(oldState_.width, oldState_.height);
            for (int y = 0; y < oldState_.height; ++y) {
                for (int x = 0; x < oldState_.width; ++x) {
                    mapChip_->SetChip(x, y, static_cast<MapChip2D::ChipType>(oldState_.data[y][x]));
                }
            }
            mapChip_->SetDirty();
        }
        void Redo() override {
            if (!mapChip_) return;
            mapChip_->Resize(newState_.width, newState_.height);
            for (int y = 0; y < newState_.height; ++y) {
                for (int x = 0; x < newState_.width; ++x) {
                    mapChip_->SetChip(x, y, static_cast<MapChip2D::ChipType>(newState_.data[y][x]));
                }
            }
            mapChip_->SetDirty();
        }
    };

    class RoomEditCommand : public MapEditorContext::IMapCommand {
        MapChip2D* mapChip_;
        MapEditorContext::RoomState oldState_;
        MapEditorContext::RoomState newState_;
    public:
        RoomEditCommand(MapChip2D* chip, const MapEditorContext::RoomState& oldS, const MapEditorContext::RoomState& newS)
            : mapChip_(chip), oldState_(oldS), newState_(newS) {}
        void Undo() override {
            if (!mapChip_) return;
            mapChip_->GetRooms() = oldState_.rooms;
        }
        void Redo() override {
            if (!mapChip_) return;
            mapChip_->GetRooms() = newState_.rooms;
        }
    };

    void CaptureMapState(MapChip2D* mapChip, MapEditorContext::MapState& state) {
        if (!mapChip) return;
        state.width = mapChip->GetWidth();
        state.height = mapChip->GetHeight();
        state.data.clear();
        for (int y = 0; y < state.height; ++y) {
            std::vector<int> row;
            for (int x = 0; x < state.width; ++x) {
                row.push_back(static_cast<int>(mapChip->GetChip(x, y)));
            }
            state.data.push_back(row);
        }
    }
}

MapEditorContext::MapEditorContext() {
}

void MapEditorContext::Initialize() {
    ScanAvailableModels();
    ScanAvailableTextures();
}

void MapEditorContext::ScanAvailableModels() {
    availableModels_.clear();
    std::filesystem::path basePath("resources/Object");
    if (!std::filesystem::exists(basePath) || !std::filesystem::is_directory(basePath)) return;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(basePath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".obj") {
            std::string relativePath = std::filesystem::relative(entry.path(), "resources").string();
            std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
            availableModels_.push_back(relativePath);
        }
    }
}

void MapEditorContext::ScanAvailableTextures() {
    availableTextures_.clear();
    std::filesystem::path basePath("resources");
    if (!std::filesystem::exists(basePath) || !std::filesystem::is_directory(basePath)) return;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(basePath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".png" || ext == ".jpg" || ext == ".dds" || ext == ".tga") {
                std::string relativePath = std::filesystem::relative(entry.path(), "resources").string();
                std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
                availableTextures_.push_back(relativePath);
            }
        }
    }
}

void MapEditorContext::SetStageFilename(const std::string& filename) {
    strcpy_s(stageFilename_, sizeof(stageFilename_), filename.c_str());
}

std::string MapEditorContext::GetFullFilePath(const char* filename) const {
    std::string name = filename;
    bool hasExt = false;
    if (name.length() >= 4) {
        std::string ext = name.substr(name.length() - 4);
        if (ext == ".txt" || ext == ".TXT") {
            hasExt = true;
        }
    }
    if (!hasExt) {
        name += ".txt";
    }
    return std::string("resources/json/shared/MapData/") + name;
}

void MapEditorContext::PushCommand(std::shared_ptr<IMapCommand> cmd) {
    undoStack_.push_back(cmd);
    if (undoStack_.size() > 100) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
}

void MapEditorContext::Undo() {
    if (undoStack_.empty()) return;
    auto cmd = undoStack_.back();
    undoStack_.pop_back();
    cmd->Undo();
    redoStack_.push_back(cmd);
}

void MapEditorContext::Redo() {
    if (redoStack_.empty()) return;
    auto cmd = redoStack_.back();
    redoStack_.pop_back();
    cmd->Redo();
    undoStack_.push_back(cmd);
}

void MapEditorContext::ClearHistory() {
    undoStack_.clear();
    redoStack_.clear();
}

void MapEditorContext::BeginMapHistoryCapture(MapChip2D* mapChip) {
    if (!mapChip) return;
    CaptureMapState(mapChip, oldMapState_);
}

void MapEditorContext::EndMapHistoryCapture(MapChip2D* mapChip) {
    if (!mapChip) return;
    MapState newState;
    CaptureMapState(mapChip, newState);
    if (oldMapState_.width != newState.width || oldMapState_.height != newState.height || oldMapState_.data != newState.data) {
        PushCommand(std::make_shared<MapEditCommand>(mapChip, oldMapState_, newState));
    }
}

void MapEditorContext::BeginRoomHistoryCapture(MapChip2D* mapChip) {
    if (!mapChip) return;
    oldRoomState_.rooms = mapChip->GetRooms();
}

void MapEditorContext::EndRoomHistoryCapture(MapChip2D* mapChip) {
    if (!mapChip) return;
    RoomState newState;
    newState.rooms = mapChip->GetRooms();
    bool changed = false;
    if (oldRoomState_.rooms.size() != newState.rooms.size()) {
        changed = true;
    } else {
        for (size_t i = 0; i < newState.rooms.size(); ++i) {
            if (oldRoomState_.rooms[i].x != newState.rooms[i].x ||
                oldRoomState_.rooms[i].y != newState.rooms[i].y ||
                oldRoomState_.rooms[i].width != newState.rooms[i].width ||
                oldRoomState_.rooms[i].height != newState.rooms[i].height) {
                changed = true;
                break;
            }
        }
    }
    if (changed) {
        PushCommand(std::make_shared<RoomEditCommand>(mapChip, oldRoomState_, newState));
    }
}

void MapEditorContext::UpdateAStarPositionsFromMap(MapChip2D* mapChip, SceneManager* sceneManager) {
    if (!mapChip) return;

    bool foundSpawn = false;
    IScene* activeScene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;

    if (activeScene) {
        Player2D* player = activeScene->GetPlayer();
        if (player) {
            Vector3 pos = player->GetStartPosition();
            if (pos.x != 0.0f || pos.y != 0.0f) {
                aStarStartPos_[0] = pos.x;
                aStarStartPos_[1] = pos.y;
                foundSpawn = true;
            } else {
                pos = player->GetPosition();
                if (pos.x != 0.0f || pos.y != 0.0f) {
                    aStarStartPos_[0] = pos.x;
                    aStarStartPos_[1] = pos.y;
                    foundSpawn = true;
                }
            }
        }
    }

    if (!foundSpawn && mapChip->HasPlayerSpawn()) {
        Vector3 spawnWorldPos = mapChip->GetPlayerSpawnWorldPosition();
        aStarStartPos_[0] = spawnWorldPos.x;
        aStarStartPos_[1] = spawnWorldPos.y;
        foundSpawn = true;
    }

    if (!foundSpawn) {
        const auto& currentReplay = ReplayManager::GetInstance()->GetCurrentReplay();
        if (currentReplay.playerInitPos.x != 0.0f || currentReplay.playerInitPos.y != 0.0f) {
            aStarStartPos_[0] = currentReplay.playerInitPos.x;
            aStarStartPos_[1] = currentReplay.playerInitPos.y;
            foundSpawn = true;
        }
    }

    if (!foundSpawn) {
        aStarStartPos_[0] = 2.0f;
        aStarStartPos_[1] = 5.0f;
    }

    // ゴール位置検索
    bool foundGoal = false;
    int mapW = mapChip->GetWidth();
    int mapH = mapChip->GetHeight();
    for (int y = 0; y < mapH; ++y) {
        for (int x = 0; x < mapW; ++x) {
            if (mapChip->GetChip(x, y) == MapChip2D::ChipType::kGoal) {
                aStarGoalPos_[0] = static_cast<float>(x) + 0.5f;
                aStarGoalPos_[1] = static_cast<float>(y) + 0.5f;
                foundGoal = true;
                break;
            }
        }
        if (foundGoal) break;
    }

    if (!foundGoal) {
        aStarGoalPos_[0] = static_cast<float>(mapW) - 3.0f;
        aStarGoalPos_[1] = 5.0f;
    }
}
#endif
