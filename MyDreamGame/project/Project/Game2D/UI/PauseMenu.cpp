#include "PauseMenu.h"
#include "Resource/Sprite/Sprite.h"
#include "Graphics/TextureManager.h"
#include "Input/KeyboardInput.h"
#include "Input/GamepadInput.h"

void PauseMenu::Initialize(SpriteCommon* spriteCommon) {
    if (!spriteCommon) return;

    whiteTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/white.png");
    continueTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/tudukeru.png");
    selectTextureHandle_ = TextureManager::GetInstance()->Load("resources/Sprite/Original/UI/serekutonimodoru.png");

    float centerX = 1280.0f * 0.5f;

    // 半透明背景オーバーレイ
    overlaySprite_ = std::make_unique<Sprite>();
    overlaySprite_->Initialize(spriteCommon, whiteTextureHandle_);
    overlaySprite_->SetPosition({ centerX - 5000.0f, 360.0f - 5000.0f });
    overlaySprite_->SetSize({ 10000.0f, 10000.0f });
    overlaySprite_->SetColor({ 0.2f, 0.2f, 0.2f, 0.65f });

    // つづける
    continueSprite_ = std::make_unique<Sprite>();
    continueSprite_->Initialize(spriteCommon, continueTextureHandle_);
    continueSprite_->SetPosition({ centerX - 200.0f, 310.0f - 40.0f });
    continueSprite_->SetSize({ 400.0f, 80.0f });
    continueSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    // セレクトに戻る
    selectSprite_ = std::make_unique<Sprite>();
    selectSprite_->Initialize(spriteCommon, selectTextureHandle_);
    selectSprite_->SetPosition({ centerX - 200.0f, 430.0f - 40.0f });
    selectSprite_->SetSize({ 400.0f, 80.0f });
    selectSprite_->SetColor({ 0.45f, 0.45f, 0.45f, 0.65f });

    selectedIndex_ = 0;
}

void PauseMenu::Reset() {
    selectedIndex_ = 0;
}

PauseMenuAction PauseMenu::Update(float /*deltaTime*/) {
    // ポーズ画面の操作（キーボード / コントローラー両対応）
    bool isMoveUp = KeyboardInput::GetInstance()->IsKeyPressed(DIK_UP) ||
                    KeyboardInput::GetInstance()->IsKeyPressed(DIK_W) ||
                    GamepadInput::GetInstance()->IsDPadPressedUp() ||
                    GamepadInput::GetInstance()->IsLeftStickPushedUp();
    bool isMoveDown = KeyboardInput::GetInstance()->IsKeyPressed(DIK_DOWN) ||
                      KeyboardInput::GetInstance()->IsKeyPressed(DIK_S) ||
                      GamepadInput::GetInstance()->IsDPadPressedDown() ||
                      GamepadInput::GetInstance()->IsLeftStickPushedDown();

    if (isMoveUp) {
        selectedIndex_ = 0;
    } else if (isMoveDown) {
        selectedIndex_ = 1;
    }

    // 決定操作（SPACE / RETURN / Pad A）
    bool isSubmit = KeyboardInput::GetInstance()->IsKeyPressed(DIK_SPACE) ||
                    KeyboardInput::GetInstance()->IsKeyPressed(DIK_RETURN) ||
                    GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::A);

    // キャンセル操作（ESC / Pad B / Pad START でゲーム画面へ戻る）
    bool isCancel = KeyboardInput::GetInstance()->IsKeyPressed(DIK_ESCAPE) ||
                    GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::B) ||
                    GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::Start);

    PauseMenuAction action = PauseMenuAction::None;
    if (isCancel) {
        action = PauseMenuAction::Resume;
    } else if (isSubmit) {
        if (selectedIndex_ == 0) {
            action = PauseMenuAction::Resume;
        } else if (selectedIndex_ == 1) {
            action = PauseMenuAction::ExitToStageSelect;
        }
    }

    // ポーズ中スプライトの更新（選択中は白色、非選択は薄色）
    const Vector4 kSelectedColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const Vector4 kUnselectedColor = { 0.45f, 0.45f, 0.45f, 0.65f };
    float centerX = 1280.0f * 0.5f;

    if (overlaySprite_) {
        overlaySprite_->SetPosition({ centerX - 5000.0f, 360.0f - 5000.0f });
        overlaySprite_->SetSize({ 10000.0f, 10000.0f });
        overlaySprite_->Update();
    }
    if (continueSprite_) {
        bool isSelected = (selectedIndex_ == 0);
        continueSprite_->SetColor(isSelected ? kSelectedColor : kUnselectedColor);
        float w = isSelected ? 410.0f : 390.0f;
        float h = isSelected ? 82.0f : 78.0f;
        continueSprite_->SetSize({ w, h });
        continueSprite_->SetPosition({ centerX - w * 0.5f, 310.0f - h * 0.5f });
        continueSprite_->Update();
    }
    if (selectSprite_) {
        bool isSelected = (selectedIndex_ == 1);
        selectSprite_->SetColor(isSelected ? kSelectedColor : kUnselectedColor);
        float w = isSelected ? 410.0f : 390.0f;
        float h = isSelected ? 82.0f : 78.0f;
        selectSprite_->SetSize({ w, h });
        selectSprite_->SetPosition({ centerX - w * 0.5f, 430.0f - h * 0.5f });
        selectSprite_->Update();
    }

    return action;
}

void PauseMenu::Draw() {
    if (overlaySprite_) overlaySprite_->Draw();
    if (continueSprite_) continueSprite_->Draw();
    if (selectSprite_) selectSprite_->Draw();
}
