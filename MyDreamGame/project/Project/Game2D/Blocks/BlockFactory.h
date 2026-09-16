#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

class BaseBlock;
class MapChip2D;

/// <summary>
/// ブロックインスタンス生成を一元管理するファクトリクラス
/// </summary>
class BlockFactory {
public:
    using BlockCreator = std::function<std::shared_ptr<BaseBlock>(MapChip2D* map, int x, int y)>;

    static BlockFactory& GetInstance();

    void Register(const std::string& typeName, BlockCreator creator);
    std::shared_ptr<BaseBlock> Create(const std::string& typeName, MapChip2D* map, int x, int y);
    const std::vector<std::string>& GetAvailableTypes() const { return registeredTypes_; }
    bool HasType(const std::string& typeName) const;

private:
    BlockFactory();
    ~BlockFactory() = default;
    BlockFactory(const BlockFactory&) = delete;
    BlockFactory& operator=(const BlockFactory&) = delete;

    std::unordered_map<std::string, BlockCreator> creators_;
    std::vector<std::string> registeredTypes_;
};

#define REGISTER_BLOCK_CLASS(ClassName) \
    namespace { \
        struct ClassName##_AutoRegister { \
            ClassName##_AutoRegister() { \
                ::BlockFactory::GetInstance().Register(#ClassName, [](MapChip2D* map, int x, int y) -> std::shared_ptr<BaseBlock> { \
                    return std::make_shared<ClassName>(map, x, y); \
                }); \
            } \
        } s_##ClassName##_AutoRegister; \
    }
