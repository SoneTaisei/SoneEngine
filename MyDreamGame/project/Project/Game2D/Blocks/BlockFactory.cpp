#include "BlockFactory.h"
#include "NormalBlock.h"
#include "DeathBlock.h"
#include "GoalBlock.h"
#include "OneWayBlock.h"
#include "CoinBlock.h"
#include "JumpBlock.h"
#include "LiftBlock.h"
#include "PatrolEnemyBlock.h"
#include "RailBlock.h"
#include <algorithm>

BlockFactory& BlockFactory::GetInstance() {
    static BlockFactory instance;
    return instance;
}

BlockFactory::BlockFactory() {
    Register("NormalBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<NormalBlock>(map, x, y);
    });
    Register("DeathBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<DeathBlock>(map, x, y);
    });
    Register("GoalBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<GoalBlock>(map, x, y);
    });
    Register("OneWayBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<OneWayBlock>(map, x, y);
    });
    Register("CoinBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<CoinBlock>(map, x, y);
    });
    Register("JumpBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<JumpBlock>(map, x, y);
    });
    Register("LiftBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<LiftBlock>(map, x, y);
    });
    Register("PatrolEnemyBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<PatrolEnemyBlock>(map, x, y);
    });
    Register("RailBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<RailBlock>(map, x, y);
    });
}

void BlockFactory::Register(const std::string& typeName, BlockCreator creator) {
    creators_[typeName] = creator;
    if (std::find(registeredTypes_.begin(), registeredTypes_.end(), typeName) == registeredTypes_.end()) {
        registeredTypes_.push_back(typeName);
    }
}

std::shared_ptr<BaseBlock> BlockFactory::Create(const std::string& typeName, MapChip2D* map, int x, int y) {
    auto it = creators_.find(typeName);
    if (it != creators_.end()) {
        return it->second(map, x, y);
    }
    return std::make_shared<NormalBlock>(map, x, y);
}

bool BlockFactory::HasType(const std::string& typeName) const {
    return creators_.find(typeName) != creators_.end();
}
