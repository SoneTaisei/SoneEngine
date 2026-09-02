#include "BlockFactory.h"
#include "NormalBlock.h"
#include "DeathBlock.h"
#include "GoalBlock.h"
#include "OneWayBlock.h"
#include "ChainItemBlock.h"
#include "MovingBlock.h"
#include "FragileBlock.h"
#include "SwitchBlock.h"
#include "DoorBlock.h"
#include <algorithm>

BlockFactory& BlockFactory::GetInstance() {
    static BlockFactory instance;
    return instance;
}

BlockFactory::BlockFactory() {
    // デフォルトブロッククラスの登録
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
    Register("ChainItemBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<ChainItemBlock>(map, x, y);
    });
    Register("MovingBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<MovingBlock>(map, x, y);
    });
    Register("FragileBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<FragileBlock>(map, x, y);
    });
    Register("SwitchBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<SwitchBlock>(map, x, y);
    });
    Register("DoorBlock", [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> {
        return std::make_shared<DoorBlock>(map, x, y);
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
    // 未登録（まだビルドされていない新クラスなど）の場合はフォールバックとしてNormalBlockを生成
    return std::make_shared<NormalBlock>(map, x, y);
}

bool BlockFactory::HasType(const std::string& typeName) const {
    return creators_.find(typeName) != creators_.end();
}
