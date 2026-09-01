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

    /// <summary>
    /// ブロッククラスのクリエーター関数を登録
    /// </summary>
    void Register(const std::string& typeName, BlockCreator creator);

    /// <summary>
    /// 指定された型名のブロックインスタンスを生成
    /// </summary>
    std::shared_ptr<BaseBlock> Create(const std::string& typeName, MapChip2D* map, int x, int y);

    /// <summary>
    /// 登録済みの全ブロック型名リストを取得
    /// </summary>
    const std::vector<std::string>& GetAvailableTypes() const { return registeredTypes_; }

    /// <summary>
    /// 指定された型名が登録されているか判定
    /// </summary>
    bool HasType(const std::string& typeName) const;

private:
    BlockFactory();
    ~BlockFactory() = default;
    BlockFactory(const BlockFactory&) = delete;
    BlockFactory& operator=(const BlockFactory&) = delete;

    std::unordered_map<std::string, BlockCreator> creators_;
    std::vector<std::string> registeredTypes_;
};

/// <summary>
/// ブロッククラスをBlockFactoryに自動登録するマクロ
/// </summary>
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
