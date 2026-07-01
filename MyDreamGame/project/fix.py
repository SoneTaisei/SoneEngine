with open('Project/Game2D/MapChip2D.h', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('    void SetDirty() { isDirty_ = true; }\n\nprivate:\n', '')
text = text.replace('private:\n    std::shared_ptr<BaseBlock> InstantiateBlock', 'public:\n    void SetDirty() { isDirty_ = true; }\nprivate:\n    std::shared_ptr<BaseBlock> InstantiateBlock')
with open('Project/Game2D/MapChip2D.h', 'w', encoding='utf-8') as f:
    f.write(text)

with open('Project/Game2D/MapChip2D.cpp', 'r', encoding='utf-8') as f:
    cpp = f.read()

inst_start = cpp.find('std::shared_ptr<BaseBlock> MapChip2D::InstantiateBlock')
if inst_start != -1:
    cpp = cpp[:inst_start]

new_inst = '''
std::shared_ptr<BaseBlock> MapChip2D::InstantiateBlock(int x, int y, ChipType type, int spanWidth, int spanHeight, Primitive* boxPrimitive) {
    float worldX = ChipToWorldX(x) + (spanWidth * chipSize_) * 0.5f;
    float worldY = ChipToWorldY(y) + (spanHeight * chipSize_) * 0.5f;

    std::shared_ptr<BaseBlock> newBlock = nullptr;
    int typeId = static_cast<int>(type);

    if (type == ChipType::kBlock) {
        newBlock = std::make_shared<NormalBlock>(this, x, y);
    } else if (type == ChipType::kDeathBlock) {
        newBlock = std::make_shared<DeathBlock>(this, x, y);
    } else if (type == ChipType::kGoal) {
        newBlock = std::make_shared<GoalBlock>(this, x, y);
    } else if (type == ChipType::kCoin) {
        newBlock = std::make_shared<CoinBlock>(this, x, y);
    } else if (type == ChipType::kOneWayBlock) {
        newBlock = std::make_shared<OneWayBlock>(this, x, y);
    } else if (type == ChipType::kLift) {
        newBlock = std::make_shared<LiftBlock>(this, x, y);
    } else if (type == ChipType::kRail) {
        newBlock = std::make_shared<RailBlock>(this, x, y);
    } else if (type == ChipType::kJumpBlock) {
        newBlock = std::make_shared<JumpBlock>(this, x, y);
    } else if (typeId >= 100) {
        const CustomBlockDef* def = nullptr;
        for (const auto& d : customPalette_) {
            if (d.id == typeId) { def = &d; break; }
        }
        if (def) {
            if (def->type == "NormalBlock") newBlock = std::make_shared<NormalBlock>(this, x, y);
            else if (def->type == "DeathBlock") newBlock = std::make_shared<DeathBlock>(this, x, y);
            else if (def->type == "GoalBlock") newBlock = std::make_shared<GoalBlock>(this, x, y);
            else if (def->type == "CoinBlock") newBlock = std::make_shared<CoinBlock>(this, x, y);
            else if (def->type == "OneWayBlock") newBlock = std::make_shared<OneWayBlock>(this, x, y);
            else if (def->type == "LiftBlock") newBlock = std::make_shared<LiftBlock>(this, x, y);
            else if (def->type == "RailBlock") newBlock = std::make_shared<RailBlock>(this, x, y);
            else if (def->type == "JumpBlock") newBlock = std::make_shared<JumpBlock>(this, x, y);
        }
    }

    if (newBlock) {
        newBlock->Initialize(device_.Get(), boxPrimitive, worldX, worldY, spanWidth * chipSize_, spanHeight * chipSize_);
        
        const CustomBlockDef* def = nullptr;
        if (typeId >= 100) {
            for (const auto& d : customPalette_) {
                if (d.id == typeId) { def = &d; break; }
            }
        } else if (typeId >= 1 && typeId <= 9) {
            for (const auto& d : templatePalette_) {
                if (d.id == typeId) { def = &d; break; }
            }
        }
        
        if (def) {
            newBlock->SetProperties(def->properties);
            if (newBlock->GetPrimitive()) {
                newBlock->GetPrimitive()->GetMaterial().color = def->color;
                
                const Transform& t = newBlock->GetPrimitive()->GetTransform();
                newBlock->GetPrimitive()->SetScale({ 
                    t.scale.x * def->scale.x, 
                    t.scale.y * def->scale.y, 
                    t.scale.z * def->scale.z 
                });
            }
            
            if (!def->modelName.empty()) {
                Model* model = nullptr;
                if (def->modelName.length() >= 4 && def->modelName.substr(def->modelName.length() - 4) == ".obj") {
                    std::string fullPath = "resources/" + def->modelName;
                    std::filesystem::path p(fullPath);
                    std::string dirPath = p.parent_path().string();
                    std::string fileName = p.filename().string();
                    std::replace(dirPath.begin(), dirPath.end(), '\\\\', '/');
                    model = ModelManager::GetInstance()->GetModel(dirPath, fileName);
                } else {
                    model = ModelManager::GetInstance()->GetModel("resources/Object/School/" + def->modelName, def->modelName + ".obj");
                    if (!model) {
                        model = ModelManager::GetInstance()->GetModel("resources/models", def->modelName + ".obj");
                    }
                }
                
                if (model) {
                    auto obj = std::make_unique<Object3D>();
                    obj->Initialize(device_.Get(), model);
                    obj->SetTranslation({ worldX, worldY, 0.0f });
                    obj->SetScale(def->scale);
                    obj->SetTextureHandle(gpuHandle_);
                    obj->GetMaterial().color = def->color;
                    newBlock->SetObject3D(std::move(obj));
                }
            }
        }

        if (newBlock->GetPrimitive()) {
            newBlock->GetPrimitive()->SetName("MapChip_" + std::to_string(x) + "_" + std::to_string(y));
            if (spanWidth > 1 || spanHeight > 1) {
                Matrix4x4 uvTrans = TransformFunctions::MakeScaleMatrix({(float)spanWidth, (float)spanHeight, 1.0f});
                newBlock->GetPrimitive()->GetMaterial().uvTransform = uvTrans;
            }
        }
    }
    return newBlock;
}
'''
cpp += new_inst
with open('Project/Game2D/MapChip2D.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp)
