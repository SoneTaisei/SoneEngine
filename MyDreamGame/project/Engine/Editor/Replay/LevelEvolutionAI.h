#pragma once
#include "Core/Utility/Structs.h"
#include <vector>
#include <string>
#include <random>

class MapChip2D;
class ReplayManager;

struct GimmickGene {
    int type = 0; // 0: 空白, 1: トゲ, 2: 動く床(リフト)
    float posX = 0.0f;
    float posY = 0.0f;
    int chipX = 0;
    int chipY = 0;
};

struct StageChromosome {
    std::vector<GimmickGene> gimmicks;
    float fitness = 0.0f;
};

class LevelEvolutionAI {
public:
    LevelEvolutionAI() = default;
    ~LevelEvolutionAI() = default;

    // 進化プロセスを実行するメインメソッド
    void RunEvolution(MapChip2D* mapChip, ReplayManager* replayMgr, float targetDifficulty, int generations = 30);

    const std::vector<std::string>& GetEvolutionLogs() const { return evolutionLogs_; }
    void ClearLogs() { evolutionLogs_.clear(); }

private:
    // マップからギミック配置可能なグリッドを自動選定
    void InitializeCandidates(MapChip2D* mapChip);
    
    // 染色体（配置データ）をマップチップに適用
    void ApplyChromosomeToMap(const StageChromosome& chromosome, MapChip2D* mapChip);
    
    // 適応度評価
    void EvaluatePopulation(MapChip2D* mapChip, ReplayManager* replayMgr, float targetDifficulty);
    
    // 次世代生成（選択・交叉・突然変異）
    void EvolveNextGeneration();
    
    StageChromosome SelectParent();
    StageChromosome Crossover(const StageChromosome& parentA, const StageChromosome& parentB);
    void Mutate(StageChromosome& chromosome);

private:
    struct CandidatePos {
        float x, y;
        int chipX, chipY;
    };
    std::vector<CandidatePos> candidates_;
    std::vector<StageChromosome> population_;
    std::vector<std::string> evolutionLogs_;
    
    std::string originalMapDataStr_ = ""; // ベースマップ復元用

    const int POPULATION_SIZE = 40;
    const float MUTATION_RATE = 0.08f; // 8%の確率で突然変異
};
