#pragma once
#ifdef USE_IMGUI
#include <functional>
#include <memory>
#include <vector>

class EditorManager;
class SceneManager;

// エディターのUndo/Redoコマンド基底クラス
class IEditorCommand {
public:
    virtual ~IEditorCommand() = default;
    virtual void Undo() = 0;
    virtual void Redo() = 0;
};

template <typename T>
class ValueEditCommand : public IEditorCommand {
public:
    ValueEditCommand(T *target, const T &oldVal, const T &newVal, std::function<void()> onUpdate = nullptr)
        : target_(target), oldValue_(oldVal), newValue_(newVal), onUpdate_(onUpdate) {}

    void Undo() override {
        if (target_) {
            *target_ = oldValue_;
            if (onUpdate_) {
                onUpdate_();
            }
        }
    }
    void Redo() override {
        if (target_) {
            *target_ = newValue_;
            if (onUpdate_) {
                onUpdate_();
            }
        }
    }

private:
    T *target_ = nullptr;
    T oldValue_;
    T newValue_;
    std::function<void()> onUpdate_;
};

// コマンド履歴（Undo/Redoスタック）管理クラス
class ActionCommand : public IEditorCommand {
public:
    ActionCommand(std::function<void()> undo, std::function<void()> redo)
        : undoAction_(undo), redoAction_(redo) {}
    void Undo() override { if (undoAction_) undoAction_(); }
    void Redo() override { if (redoAction_) redoAction_(); }
private:
    std::function<void()> undoAction_;
    std::function<void()> redoAction_;
};

// コマンド履歴（Undo/Redoスタック）管理クラス
class EditorCommandManager {
public:
    EditorCommandManager() = default;
    ~EditorCommandManager() = default;

    template <typename T>
    void PushValueCommand(T *target, const T &oldVal, const T &newVal, std::function<void()> onUpdate = nullptr) {
        PushCommand(std::make_shared<ValueEditCommand<T>>(target, oldVal, newVal, onUpdate));
    }

    void PushActionCommand(std::function<void()> undo, std::function<void()> redo) {
        PushCommand(std::make_shared<ActionCommand>(undo, redo));
    }

    void PushCommand(std::shared_ptr<IEditorCommand> cmd) {
        undoStack_.push_back(cmd);
        if (undoStack_.size() > kMaxHistorySize) {
            undoStack_.erase(undoStack_.begin());
        }
        redoStack_.clear();
    }

    void Undo();
    void Redo();
    void Clear() {
        undoStack_.clear();
        redoStack_.clear();
    }
    bool CanUndo() const { return !undoStack_.empty(); }
    bool CanRedo() const { return !redoStack_.empty(); }

    void UpdateShortcuts(EditorManager *em, SceneManager *sceneManager);

private:
    static constexpr size_t kMaxHistorySize = 100;
    std::vector<std::shared_ptr<IEditorCommand>> undoStack_;
    std::vector<std::shared_ptr<IEditorCommand>> redoStack_;
};

#endif // USE_IMGUI
