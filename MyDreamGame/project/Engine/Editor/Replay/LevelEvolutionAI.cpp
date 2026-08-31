#include "LevelEvolutionAI.h"
#include "Game2D/MapChip2D.h"
#include "ReplayManager.h"
#include "Core/Utility/LogManager.h"
#include <algorithm>
#include <ctime>
#include <cmath>
#include <format>

void LevelEvolutionAI::RunEvolution(MapChip2D* mapChip, ReplayManager* replayMgr, float targetDifficulty, int generations) {
    evolutionLogs_.clear();
    if (!mapChip || !replayMgr) {
        evolutionLogs_.push_back("[ERROR] マップまたはリプレイマネージャーが無効です。");
        return;
    }

    const auto& perfectMacro = replayMgr->GetCurrentReplay().frames;
    if (perfectMacro.empty()) {
        std::string warnMsg = "自動進化を開始できません: ベースとなるお手本リプレイデータが空です。";
        evolutionLogs_.push_back("[WARN] " + warnMsg);
        LogManager::GetInstance()->AddLog(LogLevel::Warning, warnMsg);
        return;
    }

    // 元のマップデータを退避
    originalMapDataStr_ = mapChip->GetMapDataAsString();

    // 1. ギミック候補地の選定
    InitializeCandidates(mapChip);
    if (candidates_.empty()) {
        std::string warnMsg = "自動進化を開始できません: ギミックを配置できる適切な候補地が見つかりませんでした。";
        evolutionLogs_.push_back("[WARN] " + warnMsg);
        LogManager::GetInstance()->AddLog(LogLevel::Warning, warnMsg);
        return;
    }

    evolutionLogs_.push_back("[INFO] レベル進化AI開始 (世代数: " + std::to_string(generations) + 
                             ", 候補地数: " + std::to_string(candidates_.size()) + 
                             ", 目標難易度: " + std::to_string((int)targetDifficulty) + ")");

    // 2. 初期個体群の生成
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> gimmickDist(0, 2); // 0:なし, 1:トゲ, 2:リフト

    population_.clear();
    for (int i = 0; i < POPULATION_SIZE; ++i) {
        StageChromosome chromosome;
        for (const auto& cand : candidates_) {
            GimmickGene gene;
            gene.type = gimmickDist(gen);
            gene.posX = cand.x;
            gene.posY = cand.y;
            gene.chipX = cand.chipX;
            gene.chipY = cand.chipY;
            chromosome.gimmicks.push_back(gene);
        }
        population_.push_back(chromosome);
    }

    StageChromosome bestOverall;
    bestOverall.fitness = -1.0f;

    // 3. 進化のメインループ
    for (int g = 0; g < generations; ++g) {
        // 適応度評価
        EvaluatePopulation(mapChip, replayMgr, targetDifficulty);

        // 最優秀個体の特定
        float maxFit = -1.0f;
        size_t bestIdx = 0;
        for (size_t i = 0; i < population_.size(); ++i) {
            if (population_[i].fitness > maxFit) {
                maxFit = population_[i].fitness;
                bestIdx = i;
            }
        }

        if (maxFit > bestOverall.fitness) {
            bestOverall = population_[bestIdx];
        }

        // ログ出力
        // 最優秀個体の難易度を一時適用して測定してみる
        ApplyChromosomeToMap(population_[bestIdx], mapChip);
        std::vector<FrameData> simResult = replayMgr->SimulateMacro(perfectMacro, mapChip);
        DifficultyScore score = replayMgr->AnalyzeReplayDifficulty(simResult, mapChip);
        bool cleared = (simResult.size() >= perfectMacro.size());

        std::string statusStr = cleared ? "クリア可能" : "クリア不可(スタック/死亡)";
        std::string logMsg = "世代 #" + std::to_string(g + 1) + " - 最高適応度: " + std::to_string((int)maxFit) + 
                             " (難易度: " + std::format("{:.1f}", score.finalCalculatedDifficulty) + " / " + statusStr + ")";
        evolutionLogs_.push_back(logMsg);
        LogManager::GetInstance()->AddLog(LogLevel::Info, "LevelEvolutionAI: " + logMsg);

        // 次世代の生成
        if (g < generations - 1) {
            EvolveNextGeneration();
        }
    }

    // 4. 最優秀個体を最終適用
    if (bestOverall.fitness >= 0.0f) {
        ApplyChromosomeToMap(bestOverall, mapChip);
        // マップのグラフィックオブジェクトを再構築して画面に反映
        mapChip->SetRebuildEnabled(true);

        std::string successMsg = "自動進化完了！最優秀個体をマップに反映しました。";
        evolutionLogs_.push_back("[SUCCESS] " + successMsg);
        LogManager::GetInstance()->AddLog(LogLevel::Info, successMsg);
    } else {
        // 失敗した場合は元のマップに戻す
        mapChip->LoadFromString(originalMapDataStr_);
        mapChip->SetRebuildEnabled(true);
        evolutionLogs_.push_back("[WARN] 有効なステージ配置が見つかりませんでした。元の配置に戻します。");
    }
}

void LevelEvolutionAI::InitializeCandidates(MapChip2D* mapChip) {
    candidates_.clear();
    if (!mapChip) return;

    // マップ全体をスキャンし、ギミック配置可能な場所を特定
    // スタート（X: 0〜5）とゴール（X: width-5〜width）付近は除外
    for (int x = 6; x < mapChip->GetWidth() - 6; ++x) {
        for (int y = 1; y < mapChip->GetHeight() - 3; ++y) {
            // 床（kBlock）があり、その上が空白（kNone）である場所を候補とする
            if (mapChip->GetChipType(x, y - 1) == MapChip2D::ChipType::kBlock &&
                mapChip->GetChipType(x, y) == MapChip2D::ChipType::kNone &&
                mapChip->GetChipType(x, y + 1) == MapChip2D::ChipType::kNone) {
                
                CandidatePos cand;
                cand.chipX = x;
                cand.chipY = y;
                cand.x = mapChip->ChipToWorldX(x) + mapChip->GetChipSize() * 0.5f;
                cand.y = mapChip->ChipToWorldY(y) + mapChip->GetChipSize() * 0.5f;
                candidates_.push_back(cand);
                
                // 隣接する列での連続配置を防ぐため、Xを1つ飛ばす
                x += 1;
                break; // 同一X列には1つのみ
            }
        }
    }

    // 候補地が多すぎるとGAの探索空間が広くなりすぎるため、最大15箇所に制限する
    if (candidates_.size() > 15) {
        std::vector<CandidatePos> filtered;
        size_t step = candidates_.size() / 15;
        for (size_t i = 0; i < candidates_.size(); i += step) {
            filtered.push_back(candidates_[i]);
            if (filtered.size() >= 15) break;
        }
        candidates_ = filtered;
    }
}

void LevelEvolutionAI::ApplyChromosomeToMap(const StageChromosome& chromosome, MapChip2D* mapChip) {
    if (!mapChip) return;

    // 一旦ベースマップデータにリセット
    mapChip->LoadFromString(originalMapDataStr_);

    // 染色体の各遺伝子のギミックを適用
    for (const auto& gene : chromosome.gimmicks) {
        if (gene.type == 0) {
            mapChip->SetChip(gene.chipX, gene.chipY, MapChip2D::ChipType::kNone);
        } else if (gene.type == 1) {
            mapChip->SetChip(gene.chipX, gene.chipY, MapChip2D::ChipType::kDeathBlock);
        }
    }
}

void LevelEvolutionAI::EvaluatePopulation(MapChip2D* mapChip, ReplayManager* replayMgr, float targetDifficulty) {
    const auto& perfectMacro = replayMgr->GetCurrentReplay().frames;

    for (auto& chromosome : population_) {
        // マップに配置を適用
        ApplyChromosomeToMap(chromosome, mapChip);

        // 高速再生シミュレーション
        std::vector<FrameData> simResult = replayMgr->SimulateMacro(perfectMacro, mapChip);

        // 難易度解析
        DifficultyScore score = replayMgr->AnalyzeReplayDifficulty(simResult, mapChip);

        // クリアできた（最後まで進めた）かの判定
        bool cleared = (simResult.size() >= perfectMacro.size());

        float fitness = 0.0f;
        if (!cleared) {
            // クリアできなかった場合は進行度（進めた割合）に応じて適応度を与える（最大20点）
            float progress = static_cast<float>(simResult.size()) / perfectMacro.size();
            fitness = progress * 20.0f;
        } else {
            // クリアできた場合は目標難易度との絶対値差を評価（最大100点、最低30点）
            float diff = std::abs(score.finalCalculatedDifficulty - targetDifficulty);
            fitness = 30.0f + (70.0f / (diff + 1.0f));
        }

        chromosome.fitness = fitness;
    }
}

void LevelEvolutionAI::EvolveNextGeneration() {
    std::vector<StageChromosome> nextGen;
    nextGen.reserve(POPULATION_SIZE);

    // 1. エリート保存（優秀な上位2個体をそのまま残す）
    std::sort(population_.begin(), population_.end(), [](const StageChromosome& a, const StageChromosome& b) {
        return a.fitness > b.fitness;
    });
    nextGen.push_back(population_[0]);
    nextGen.push_back(population_[1]);

    // 2. 次世代の生成
    while (nextGen.size() < static_cast<size_t>(POPULATION_SIZE)) {
        StageChromosome parentA = SelectParent();
        StageChromosome parentB = SelectParent();

        // 交叉
        StageChromosome child = Crossover(parentA, parentB);

        // 突然変異
        Mutate(child);

        nextGen.push_back(child);
    }

    population_ = nextGen;
}

StageChromosome LevelEvolutionAI::SelectParent() {
    // ルーレット選択
    float totalFitness = 0.0f;
    for (const auto& ch : population_) {
        totalFitness += ch.fitness;
    }

    if (totalFitness <= 0.0f) {
        // 適応度が全員0の場合はランダム選択
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dist(0, population_.size() - 1);
        return population_[dist(gen)];
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, totalFitness);
    float slice = dist(gen);

    float currentSum = 0.0f;
    for (const auto& ch : population_) {
        currentSum += ch.fitness;
        if (currentSum >= slice) {
            return ch;
        }
    }

    return population_.back();
}

StageChromosome LevelEvolutionAI::Crossover(const StageChromosome& parentA, const StageChromosome& parentB) {
    StageChromosome child = parentA;
    
    // 一点交叉
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, parentA.gimmicks.size() - 1);
    size_t crossoverPoint = dist(gen);

    for (size_t i = crossoverPoint; i < parentA.gimmicks.size(); ++i) {
        child.gimmicks[i] = parentB.gimmicks[i];
    }

    return child;
}

void LevelEvolutionAI::Mutate(StageChromosome& chromosome) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> probDist(0.0f, 1.0f);
    std::uniform_int_distribution<int> typeDist(0, 2);

    for (auto& gene : chromosome.gimmicks) {
        if (probDist(gen) < MUTATION_RATE) {
            gene.type = typeDist(gen);
        }
    }
}
