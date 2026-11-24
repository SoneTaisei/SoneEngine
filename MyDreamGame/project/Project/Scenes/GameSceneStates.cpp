#include "GameSceneStates.h"
#include "Graphics/Camera/GameCamera.h"
#include "Input/GamepadInput.h"
#include "Input/KeyboardInput.h"
#include "Scene/SceneFactory.h"
#include "Scene/SceneManager.h"
#include "Scenes/GameScene.h"

// --- StartReady ---
void GameStartReadyState::Enter(GameScene *owner) {
    if (owner->GetReadyGoUI()) {
        owner->GetReadyGoUI()->Start();
    }
}

void GameStartReadyState::Update(GameScene *owner, float deltaTime) {
    if (auto *ui = owner->GetReadyGoUI()) {
        ui->Update();
        if (ui->IsFinished()) {
            owner->ChangeState(std::make_unique<GamePlayingState>());
        }
    }
}

void GameStartReadyState::Exit(GameScene *owner) {
    if (auto *ui = owner->GetReadyGoUI()) {
        ui->Reset();
    }
}

void GameStartReadyState::Draw2D(GameScene *owner) {
    if (auto *ui = owner->GetReadyGoUI()) {
        ui->Draw();
    }
}

bool GameStartReadyState::CanControl(GameScene *owner) const {
    return owner->GetReadyGoUI() ? owner->GetReadyGoUI()->CanControl() : true;
}

// --- Playing ---
void GamePlayingState::Update(GameScene *owner, float deltaTime) {
    (void)deltaTime;
    // ポーズボタン判定
    if (KeyboardInput::GetInstance()->IsKeyPressed(DIK_ESCAPE) ||
        GamepadInput::GetInstance()->IsButtonPressed(GamepadButton::Start)) {
        owner->ChangeState(std::make_unique<GamePauseState>());
        return;
    }
    // ゴール判定
    if (owner->GetPlayer() && owner->GetPlayer()->IsGoalComplete()) {
        owner->ChangeState(std::make_unique<GameClearState>());
        return;
    }
}

// --- Pause ---
void GamePauseState::Enter(GameScene *owner) {
    if (auto *menu = owner->GetPauseMenu()) {
        menu->Reset();
    }
}

void GamePauseState::Update(GameScene *owner, float deltaTime) {
    if (auto *menu = owner->GetPauseMenu()) {
        menu->Update();
        PauseMenuAction action = menu->GetAction();
        if (action == PauseMenuAction::Resume) {
            owner->ChangeState(std::make_unique<GamePlayingState>());
        } else if (action == PauseMenuAction::ExitToStageSelect) {
            if (owner->GetCameraController()) {
                owner->GetCameraController()->ResetView();
            }
            if (owner->GetSceneManager()) {
                owner->GetSceneManager()->ChangeScene(SceneFactory::CreateScene(SceneType::kStageSelect));
            }
        }
    }
}

void GamePauseState::Draw2D(GameScene *owner) {
    if (auto *menu = owner->GetPauseMenu()) {
        menu->Draw();
    }
}

// --- Clear ---
void GameClearState::Enter(GameScene *owner) {
    if (auto *ui = owner->GetClearUI()) {
        float scale = owner->GetGameCamera() ? owner->GetGameCamera()->GetScale()
                                             : (owner->GetCameraController() ? owner->GetCameraController()->GetInitialScale() : 1.0f);
        ui->Start(scale);
        ui->SetCamera(owner->GetGameCamera());
    }
}

void GameClearState::Update(GameScene *owner, float deltaTime) {
    if (auto *ui = owner->GetClearUI()) {
        ui->Update();
        if (ui->IsReturnRequested() && owner->GetSceneManager()) {
            ui->Reset(); // ← Update() から Reset() に戻す！
            owner->GetSceneManager()->ChangeScene(SceneFactory::CreateScene(SceneType::kTitle));
        }
    }
}

void GameClearState::Exit(GameScene *owner) {
    if (auto *ui = owner->GetClearUI()) {
        ui->Reset();
    }
}

void GameClearState::Draw2D(GameScene *owner) {
    if (auto *ui = owner->GetClearUI()) {
        ui->Draw();
    }
}