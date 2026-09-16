#include "BlockClassGenerator.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <iomanip>

bool BlockClassGenerator::IsValidClassName(const std::string& name, std::string& outErrorMessage) {
    if (name.empty()) {
        outErrorMessage = "クラス名が空です。";
        return false;
    }
    if (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_') {
        outErrorMessage = "クラス名の先頭は英字またはアンダースコアである必要があります。";
        return false;
    }
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            outErrorMessage = "クラス名に使用できるのは半角英数字とアンダースコアのみです。";
            return false;
        }
    }
    return true;
}

BlockClassGenResult BlockClassGenerator::GenerateBlockClass(const BlockClassGenParams& params) {
    BlockClassGenResult result;
    std::string errorMsg;
    if (!IsValidClassName(params.className, errorMsg)) {
        result.success = false;
        result.message = errorMsg;
        return result;
    }

    std::filesystem::path blocksDir = "Project/Game2D/Blocks";
    if (!std::filesystem::exists(blocksDir)) {
        std::filesystem::create_directories(blocksDir);
    }

    std::string headerFileName = params.className + ".h";
    std::string sourceFileName = params.className + ".cpp";
    std::filesystem::path headerPath = blocksDir / headerFileName;
    std::filesystem::path sourcePath = blocksDir / sourceFileName;

    if (std::filesystem::exists(headerPath) || std::filesystem::exists(sourcePath)) {
        result.success = false;
        result.message = "同名のクラスファイルが既に存在します: " + params.className;
        return result;
    }

    // 1. ヘッダーファイルの生成
    std::ostringstream hStream;
    hStream << "#pragma once\n";
    hStream << "#include \"BaseBlock.h\"\n\n";
    hStream << "/// <summary>\n";
    hStream << "/// " << params.className << " - 自作ブロッククラス\n";
    hStream << "/// </summary>\n";
    hStream << "class " << params.className << " : public BaseBlock {\n";
    hStream << "public:\n";
    hStream << "    using BaseBlock::BaseBlock;\n\n";
    hStream << "    // 初期化処理（モデル・マテリアル・コライダーのセットアップ）\n";
    hStream << "    void Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) override;\n\n";
    hStream << "    // 毎フレームの更新処理（タイマー・移動・アニメーションなど）\n";
    hStream << "    void Update() override;\n\n";
    hStream << "    // 当たり判定の性質\n";
    hStream << "    bool IsSolid() const override { return " << (params.isSolid ? "true" : "false") << "; }\n";
    hStream << "    bool IsOneWay() const override { return " << (params.isOneWay ? "true" : "false") << "; }\n\n";
    hStream << "    // プレイヤーが接触した瞬間の処理\n";
    hStream << "    void OnCollision(Player2D* player) override;\n\n";
    hStream << "    // プレイヤーがブロックの上に乗っている時の毎フレーム処理\n";
    hStream << "    void OnPlayerStand() override;\n\n";
    hStream << "    // プレイヤーが横や下から触れた時の処理\n";
    hStream << "    void OnPlayerTouch() override;\n\n";
    hStream << "    // JSONプロパティの読み込み（エディタのインスペクターからのパラメータ設定）\n";
    hStream << "    void SetProperties(const nlohmann::json& properties) override;\n\n";
    hStream << "    // ステージ再開時・リトライ時の状態リセット処理\n";
    hStream << "    void Reset() override;\n\n";
    hStream << "#ifdef USE_IMGUI\n";
    hStream << "    // ImGuiによるリアルタイムパラメータ調整・デバッグ用UI関数\n";
    hStream << "    void DrawImGui() override;\n";
    hStream << "#endif\n\n";
    hStream << "private:\n";
    hStream << "    // --- カスタムパラメータ例 ---\n";
    hStream << "    float customPower_ = 10.0f;     // パワー・強さ\n";
    hStream << "    float speed_ = 2.0f;           // 移動・アニメ速度\n";
    hStream << "    float timer_ = 0.0f;           // 内部タイマー\n";
    hStream << "    bool isActive_ = true;         // 有効/無効フラグ\n";
    hStream << "};\n";

    {
        std::ofstream hOfs(headerPath.string());
        if (!hOfs.is_open()) {
            result.success = false;
            result.message = "ヘッダーファイルの作成に失敗しました: " + headerPath.string();
            return result;
        }
        hOfs << hStream.str();
    }

    // 2. ソースファイルの生成
    std::ostringstream cppStream;
    cppStream << "#include \"" << headerFileName << "\"\n";
    cppStream << "#include \"BlockFactory.h\"\n";
    cppStream << "#include \"Game2D/Player/Player2D.h\"\n";
    cppStream << "#ifdef USE_IMGUI\n";
    cppStream << "#include <imgui.h>\n";
    cppStream << "#endif\n\n";
    cppStream << "// BlockFactoryへの自動登録マクロ（プロジェクト起動時に登録されます）\n";
    cppStream << "REGISTER_BLOCK_CLASS(" << params.className << ");\n\n";
    cppStream << "void " << params.className << "::Initialize(ID3D12Device* device, Primitive* boxPrimitive, float worldX, float worldY, float width, float height) {\n";
    cppStream << "    gameObject_ = std::make_unique<GameObject>(\"" << params.className << "\");\n";
    cppStream << "    auto* tc = gameObject_->AddComponent<TransformComponent>();\n";
    cppStream << "    auto* prc = gameObject_->AddComponent<PrimitiveRendererComponent>();\n\n";
    cppStream << "    prc->Initialize(device, boxPrimitive);\n";
    cppStream << "    prc->GetMaterial().color = { " 
              << std::fixed << std::setprecision(2) << params.color.x << "f, " 
              << params.color.y << "f, " 
              << params.color.z << "f, " 
              << params.color.w << "f };\n";
    cppStream << "    tc->SetScale({ width, height, 1.0f });\n";
    cppStream << "    tc->SetPosition({ worldX, worldY, 0.0f });\n";
    cppStream << "    prc->GetMaterial().lightingType = 1;\n\n";
    cppStream << "    SetupCollider();\n";
    cppStream << "}\n\n";
    cppStream << "void " << params.className << "::Update() {\n";
    cppStream << "    // 基底クラスの更新（GameObjectの更新と描画キューへの登録）\n";
    cppStream << "    BaseBlock::Update();\n\n";
    cppStream << "    if (!isActive_) return;\n\n";
    cppStream << "    // TODO: ここに毎フレームの更新ロジックを記述します\n";
    cppStream << "    timer_ += 1.0f / 60.0f;\n";
    cppStream << "}\n\n";
    cppStream << "void " << params.className << "::OnCollision(Player2D* player) {\n";
    cppStream << "    // TODO: プレイヤーが接触した瞬間の処理（ダメージ、反発、アイテム取得など）\n";
    cppStream << "}\n\n";
    cppStream << "void " << params.className << "::OnPlayerStand() {\n";
    cppStream << "    // TODO: プレイヤーがこのブロックの上に乗っている間の処理（ジャンプ台、加速床、崩れる足場など）\n";
    cppStream << "}\n\n";
    cppStream << "void " << params.className << "::OnPlayerTouch() {\n";
    cppStream << "    // TODO: プレイヤーが横や下から触れた時の処理\n";
    cppStream << "}\n\n";
    cppStream << "void " << params.className << "::SetProperties(const nlohmann::json& properties) {\n";
    cppStream << "    // エディタのインスペクターからプロパティを読み込む処理\n";
    cppStream << "    if (properties.contains(\"customPower\") && properties[\"customPower\"].is_number()) {\n";
    cppStream << "        customPower_ = properties[\"customPower\"].get<float>();\n";
    cppStream << "    }\n";
    cppStream << "    if (properties.contains(\"speed\") && properties[\"speed\"].is_number()) {\n";
    cppStream << "        speed_ = properties[\"speed\"].get<float>();\n";
    cppStream << "    }\n";
    cppStream << "}\n\n";
    cppStream << "void " << params.className << "::Reset() {\n";
    cppStream << "    // TODO: ステージ再開時・リトライ時の状態初期化\n";
    cppStream << "    timer_ = 0.0f;\n";
    cppStream << "    isActive_ = true;\n";
    cppStream << "}\n\n";
    cppStream << "#ifdef USE_IMGUI\n";
    cppStream << "void " << params.className << "::DrawImGui() {\n";
    cppStream << "    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), \"[%s ロジック調整]\", \"" << params.className << "\");\n";
    cppStream << "    ImGui::DragFloat(\"カスタムパワー\", &customPower_, 0.1f, 0.0f, 100.0f);\n";
    cppStream << "    ImGui::DragFloat(\"スピード\", &speed_, 0.1f, 0.0f, 50.0f);\n";
    cppStream << "    ImGui::Checkbox(\"有効フラグ\", &isActive_);\n";
    cppStream << "    ImGui::Text(\"内部タイマー: %.2f 秒\", timer_);\n";
    cppStream << "    if (ImGui::Button(\"状態リセット (Reset)\")) {\n";
    cppStream << "        Reset();\n";
    cppStream << "    }\n";
    cppStream << "}\n";
    cppStream << "#endif\n";

    {
        std::ofstream cppOfs(sourcePath.string());
        if (!cppOfs.is_open()) {
            result.success = false;
            result.message = "ソースファイルの作成に失敗しました: " + sourcePath.string();
            return result;
        }
        cppOfs << cppStream.str();
    }

    // 3. Visual Studio プロジェクトファイルへの登録
    AddFileToVcxproj("MyDreamGame.vcxproj", "Project\\Game2D\\Blocks\\" + sourceFileName, false);
    AddFileToVcxproj("MyDreamGame.vcxproj", "Project\\Game2D\\Blocks\\" + headerFileName, true);

    AddFileToFilters("MyDreamGame.vcxproj.filters", "Project\\Game2D\\Blocks\\" + sourceFileName, "Project\\Game2D\\Blocks", false);
    AddFileToFilters("MyDreamGame.vcxproj.filters", "Project\\Game2D\\Blocks\\" + headerFileName, "Project\\Game2D\\Blocks", true);

    result.success = true;
    result.headerPath = headerPath.string();
    result.sourcePath = sourcePath.string();
    result.message = "クラス " + params.className + " を正常に生成しました。\nVisual Studioでコードを編集してビルドすることで反映されます。";
    return result;
}

bool BlockClassGenerator::AddFileToVcxproj(const std::string& vcxprojPath, const std::string& relativePath, bool isHeader) {
    std::ifstream ifs(vcxprojPath);
    if (!ifs.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    std::string checkStr = "Include=\"" + relativePath + "\"";
    if (content.find(checkStr) != std::string::npos) {
        return true; // 既に登録済み
    }

    std::string tag = isHeader ? "<ClInclude Include=\"" : "<ClCompile Include=\"";
    std::string entry = "    " + tag + relativePath + "\" />\n";

    // 最後の </ItemGroup> の手前、あるいは既存の ClCompile/ClInclude の ItemGroup 内に挿入
    std::string targetTag = isHeader ? "</ClInclude>" : "</ClCompile>";
    size_t pos = content.rfind(targetTag);
    if (pos != std::string::npos) {
        size_t lineEnd = content.find("\n", pos);
        if (lineEnd != std::string::npos) {
            content.insert(lineEnd + 1, entry);
        } else {
            content.insert(pos, entry);
        }
    } else {
        // フォールバック: 最初の </ItemGroup> の前に挿入
        size_t itemGroupPos = content.find("</ItemGroup>");
        if (itemGroupPos != std::string::npos) {
            content.insert(itemGroupPos, entry);
        }
    }

    std::ofstream ofs(vcxprojPath);
    if (!ofs.is_open()) return false;
    ofs << content;
    return true;
}

bool BlockClassGenerator::AddFileToFilters(const std::string& filtersPath, const std::string& relativePath, const std::string& filterGroup, bool isHeader) {
    std::ifstream ifs(filtersPath);
    if (!ifs.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    std::string checkStr = "Include=\"" + relativePath + "\"";
    if (content.find(checkStr) != std::string::npos) {
        return true; // 既に登録済み
    }

    std::string tag = isHeader ? "ClInclude" : "ClCompile";
    std::ostringstream entry;
    entry << "    <" << tag << " Include=\"" << relativePath << "\">\n";
    entry << "      <Filter>" << filterGroup << "</Filter>\n";
    entry << "    </" << tag << ">\n";

    std::string targetEndTag = "</" + tag + ">";
    size_t pos = content.rfind(targetEndTag);
    if (pos != std::string::npos) {
        size_t lineEnd = content.find("\n", pos);
        if (lineEnd != std::string::npos) {
            content.insert(lineEnd + 1, entry.str());
        } else {
            content.insert(pos, entry.str());
        }
    } else {
        size_t itemGroupPos = content.find("</ItemGroup>");
        if (itemGroupPos != std::string::npos) {
            content.insert(itemGroupPos, entry.str());
        }
    }

    std::ofstream ofs(filtersPath);
    if (!ofs.is_open()) return false;
    ofs << content;
    return true;
}

bool BlockClassGenerator::DeleteBlockClass(const std::string& className, std::string& outMessage) {
    if (className.empty()) {
        outMessage = "クラス名が無効です。";
        return false;
    }

    // コアブロッククラスは削除から保護
    if (className == "NormalBlock" || className == "DeathBlock" || className == "GoalBlock" || className == "OneWayBlock") {
        outMessage = "コアクラス（" + className + "）は基本機能のため、C++ファイルは保護され、テンプレート設定のみ削除されました。";
        return true;
    }

    std::filesystem::path blocksDir = "Project/Game2D/Blocks";
    std::string headerFileName = className + ".h";
    std::string sourceFileName = className + ".cpp";
    std::filesystem::path headerPath = blocksDir / headerFileName;
    std::filesystem::path sourcePath = blocksDir / sourceFileName;

    if (std::filesystem::exists(headerPath)) {
        std::filesystem::remove(headerPath);
    }
    if (std::filesystem::exists(sourcePath)) {
        std::filesystem::remove(sourcePath);
    }

    // Visual Studio プロジェクトから登録解除
    RemoveFileFromVcxproj("MyDreamGame.vcxproj", "Project\\Game2D\\Blocks\\" + sourceFileName);
    RemoveFileFromVcxproj("MyDreamGame.vcxproj", "Project\\Game2D\\Blocks\\" + headerFileName);

    RemoveFileFromFilters("MyDreamGame.vcxproj.filters", "Project\\Game2D\\Blocks\\" + sourceFileName);
    RemoveFileFromFilters("MyDreamGame.vcxproj.filters", "Project\\Game2D\\Blocks\\" + headerFileName);

    outMessage = "クラス " + className + " のファイル（.h / .cpp）およびプロジェクト登録を削除しました。";
    return true;
}

bool BlockClassGenerator::RemoveFileFromVcxproj(const std::string& vcxprojPath, const std::string& relativePath) {
    std::ifstream ifs(vcxprojPath);
    if (!ifs.is_open()) return false;

    std::string line;
    std::ostringstream newContent;
    bool found = false;

    while (std::getline(ifs, line)) {
        if (line.find(relativePath) != std::string::npos) {
            found = true;
            continue; // この行を除外
        }
        newContent << line << "\n";
    }
    ifs.close();

    if (!found) return true;

    std::ofstream ofs(vcxprojPath);
    if (!ofs.is_open()) return false;
    ofs << newContent.str();
    return true;
}

bool BlockClassGenerator::RemoveFileFromFilters(const std::string& filtersPath, const std::string& relativePath) {
    std::ifstream ifs(filtersPath);
    if (!ifs.is_open()) return false;

    std::string line;
    std::ostringstream newContent;
    bool skippingBlock = false;
    bool found = false;

    while (std::getline(ifs, line)) {
        if (!skippingBlock) {
            if (line.find(relativePath) != std::string::npos) {
                found = true;
                if (line.find("/>") == std::string::npos && (line.find("<ClCompile") != std::string::npos || line.find("<ClInclude") != std::string::npos)) {
                    skippingBlock = true; // 閉じタグまでスキップ開始
                }
                continue;
            }
            newContent << line << "\n";
        } else {
            if (line.find("</ClCompile>") != std::string::npos || line.find("</ClInclude>") != std::string::npos) {
                skippingBlock = false; // スキップ終了
            }
        }
    }
    ifs.close();

    if (!found) return true;

    std::ofstream ofs(filtersPath);
    if (!ofs.is_open()) return false;
    ofs << newContent.str();
    return true;
}
