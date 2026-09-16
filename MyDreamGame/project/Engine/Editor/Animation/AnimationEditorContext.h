#pragma once
#ifdef USE_IMGUI
#include <Windows.h>
#include <d3d12.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <optional>
#include <cmath>
#include <algorithm>
#include <imgui.h>
#include "Core/Utility/Vector3.h"
#include "Core/Utility/Matrix4x4.h"
#include "Core/Utility/Quaternion.h"
#include "Core/Utility/Animation.h"
#include "Core/Utility/TransformFunctions.h"
#include "Resource/Model/ModelCommon.h"

class SceneManager;
class AnimatorComponent;
class Object3D;
class GameObject;
class PrimitiveObject;
struct Skeleton;

// ボーン階層ツリー構造用
struct AnimJointTreeNode {
    std::string name;
    int32_t jointIndex = -1;
    int32_t parentIndex = -1;
    std::vector<int32_t> children;
    int depth = 0;
};

// 未挿入時の一時プレビューポーズ用バッファ
struct TempBoneOverride {
    std::optional<Vector3> translate;
    std::optional<Quaternion> rotate;
    std::optional<Vector3> scale;
};

// アニメーションエディター Undo / Redo スナップショット
struct AnimEditorSnapshot {
    Animation animation;
    float time = 0.0f;
    std::string selectedJointName;
    int selectedKeyIndex = -1;
    std::unordered_map<std::string, TempBoneOverride> tempOverrides;
    std::string description;
};

// --- 対称編集ヘルパー関数 ---
inline Quaternion MatrixToQuaternion(const Matrix4x4& m) {
    float tr = m.m[0][0] + m.m[1][1] + m.m[2][2];
    Quaternion q;
    if (tr > 0.0f) {
        float s = std::sqrt(tr + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m.m[1][2] - m.m[2][1]) / s;
        q.y = (m.m[2][0] - m.m[0][2]) / s;
        q.z = (m.m[0][1] - m.m[1][0]) / s;
    } else if ((m.m[0][0] > m.m[1][1]) && (m.m[0][0] > m.m[2][2])) {
        float s = std::sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]) * 2.0f;
        q.w = (m.m[1][2] - m.m[2][1]) / s;
        q.x = 0.25f * s;
        q.y = (m.m[0][1] + m.m[1][0]) / s;
        q.z = (m.m[2][0] + m.m[0][2]) / s;
    } else if (m.m[1][1] > m.m[2][2]) {
        float s = std::sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]) * 2.0f;
        q.w = (m.m[2][0] - m.m[0][2]) / s;
        q.x = (m.m[0][1] + m.m[1][0]) / s;
        q.y = 0.25f * s;
        q.z = (m.m[1][2] + m.m[2][1]) / s;
    } else {
        float s = std::sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]) * 2.0f;
        q.w = (m.m[0][1] - m.m[1][0]) / s;
        q.x = (m.m[2][0] + m.m[0][2]) / s;
        q.y = (m.m[1][2] + m.m[2][1]) / s;
        q.z = 0.25f * s;
    }
    float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 1e-6f) {
        q.x /= len; q.y /= len; q.z /= len; q.w /= len;
    }
    return q;
}

inline void DecomposeSRT(const Matrix4x4& m, Vector3& outS, Quaternion& outR, Vector3& outT) {
    outT = { m.m[3][0], m.m[3][1], m.m[3][2] };
    outS.x = std::sqrt(m.m[0][0] * m.m[0][0] + m.m[0][1] * m.m[0][1] + m.m[0][2] * m.m[0][2]);
    outS.y = std::sqrt(m.m[1][0] * m.m[1][0] + m.m[1][1] * m.m[1][1] + m.m[1][2] * m.m[1][2]);
    outS.z = std::sqrt(m.m[2][0] * m.m[2][0] + m.m[2][1] * m.m[2][1] + m.m[2][2] * m.m[2][2]);

    Matrix4x4 rotMat = m;
    if (outS.x > 1e-6f) { rotMat.m[0][0] /= outS.x; rotMat.m[0][1] /= outS.x; rotMat.m[0][2] /= outS.x; }
    if (outS.y > 1e-6f) { rotMat.m[1][0] /= outS.y; rotMat.m[1][1] /= outS.y; rotMat.m[1][2] /= outS.y; }
    if (outS.z > 1e-6f) { rotMat.m[2][0] /= outS.z; rotMat.m[2][1] /= outS.z; rotMat.m[2][2] /= outS.z; }
    rotMat.m[3][0] = rotMat.m[3][1] = rotMat.m[3][2] = 0.0f;
    rotMat.m[3][3] = 1.0f;
    outR = MatrixToQuaternion(rotMat);
}

inline bool ComputeBlenderSymmetrySRT(
    const Skeleton& skeleton,
    const std::string& srcJointName,
    const std::string& oppJointName,
    const Vector3& srcScale,
    const Quaternion& srcRot,
    const Vector3& srcTrans,
    bool axisX,
    bool axisY,
    bool axisZ,
    Vector3& outOppScale,
    Quaternion& outOppRot,
    Vector3& outOppTrans)
{
    auto itA = skeleton.jointMap.find(srcJointName);
    auto itB = skeleton.jointMap.find(oppJointName);
    if (itA == skeleton.jointMap.end() || itB == skeleton.jointMap.end()) return false;

    const auto& jA = skeleton.joints[itA->second];
    const auto& jB = skeleton.joints[itB->second];

    if (srcJointName == oppJointName) {
        outOppScale = srcScale;
        outOppTrans = srcTrans;
        if (axisX) outOppTrans.x = -outOppTrans.x;
        if (axisY) outOppTrans.y = -outOppTrans.y;
        if (axisZ) outOppTrans.z = -outOppTrans.z;

        outOppRot = srcRot;
        if (axisX) { outOppRot.y = -outOppRot.y; outOppRot.z = -outOppRot.z; }
        if (axisY) { outOppRot.x = -outOppRot.x; outOppRot.z = -outOppRot.z; }
        if (axisZ) { outOppRot.x = -outOppRot.x; outOppRot.y = -outOppRot.y; }
        return true;
    }

    Matrix4x4 M_S = TransformFunctions::MakeAffineMatrix(srcScale, srcRot, srcTrans);
    Matrix4x4 M_P = (jA.parent) ? skeleton.joints[*jA.parent].skeletonSpaceMatrix : TransformFunctions::MakeIdentity4x4();
    Matrix4x4 M_local = TransformFunctions::Multiply(M_S, M_P);

    Vector3 mx = { axisX ? -1.0f : 1.0f, axisY ? -1.0f : 1.0f, axisZ ? -1.0f : 1.0f };
    Matrix4x4 M_refl = TransformFunctions::MakeScaleMatrix(mx);

    Matrix4x4 M_mirrored = TransformFunctions::Multiply(TransformFunctions::Multiply(M_refl, M_local), M_refl);

    Matrix4x4 M_P_B = (jB.parent) ? skeleton.joints[*jB.parent].skeletonSpaceMatrix : TransformFunctions::MakeIdentity4x4();
    Matrix4x4 inv_M_P_B = TransformFunctions::Inverse(M_P_B);
    Matrix4x4 M_opp_local = TransformFunctions::Multiply(M_mirrored, inv_M_P_B);

    DecomposeSRT(M_opp_local, outOppScale, outOppRot, outOppTrans);
    return true;
}

class AnimationEditorContext {
public:
    AnimationEditorContext();
    ~AnimationEditorContext() = default;

    void Initialize();
    void UpdateAnimationPosePreview(SceneManager* sceneManager);
    void RefreshAnimationJointList(SceneManager* sceneManager);
    void ScanAnimationFiles();

    // ターゲットオブジェクト
    void SetSelectedTargets(Object3D* obj, std::shared_ptr<GameObject> gameObj, PrimitiveObject* prim) {
        selectedObject_ = obj;
        selectedGameObject_ = gameObj;
        selectedPrimitive_ = prim;
    }
    Object3D* GetSelectedObject() const { return selectedObject_; }
    std::shared_ptr<GameObject> GetSelectedGameObject() const { return selectedGameObject_; }
    PrimitiveObject* GetSelectedPrimitive() const { return selectedPrimitive_; }

    AnimatorComponent* GetTargetAnimator(SceneManager* sceneManager);

    // Undo / Redo
    void PushAnimUndoState(const std::string& desc = "");
    void BeginDragSnapshot(const std::string& desc = "");
    void PerformAnimUndo(SceneManager* sceneManager);
    void PerformAnimRedo(SceneManager* sceneManager);
    void ClearAnimUndoRedo();
    bool CanUndo() const { return !animUndoStack_.empty(); }
    bool CanRedo() const { return !animRedoStack_.empty(); }
    std::vector<AnimEditorSnapshot>& GetUndoStack() { return animUndoStack_; }
    std::vector<AnimEditorSnapshot>& GetRedoStack() { return animRedoStack_; }

    // キーフレーム挿入
    void InsertSelectedJointSRTKey(SceneManager* sceneManager);
    void InsertAllJointsSRTKey(SceneManager* sceneManager);

    // 対称編集
    std::string FindOppositeJointName(const std::string& jointName, bool axisX = true, bool axisY = false, bool axisZ = false, const Skeleton* skeleton = nullptr);

    // ゲッター / セッター (参照渡し含む)
    Animation& GetEditingAnimation() { return editingAnimation_; }
    const Animation& GetEditingAnimation() const { return editingAnimation_; }
    void SetEditingAnimation(const Animation& anim) { editingAnimation_ = anim; }

    float& GetAnimEditorTime() { return animEditorTime_; }
    float GetAnimEditorTime() const { return animEditorTime_; }
    void SetAnimEditorTime(float t) { animEditorTime_ = t; }

    bool& GetAnimEditorPlaying() { return animEditorPlaying_; }
    bool GetAnimEditorPlaying() const { return animEditorPlaying_; }
    void SetAnimEditorPlaying(bool p) { animEditorPlaying_ = p; }

    AnimationWrapMode& GetAnimEditorWrapMode() { return animEditorWrapMode_; }
    AnimationWrapMode GetAnimEditorWrapMode() const { return animEditorWrapMode_; }
    void SetAnimEditorWrapMode(AnimationWrapMode mode) { animEditorWrapMode_ = mode; }

    bool GetAnimEditorLoop() const { return animEditorWrapMode_ == AnimationWrapMode::Loop; }
    void SetAnimEditorLoop(bool loop) { animEditorWrapMode_ = loop ? AnimationWrapMode::Loop : AnimationWrapMode::Once; }

    float& GetAnimEditorFps() { return animEditorFps_; }
    float GetAnimEditorFps() const { return animEditorFps_; }

    void EnsureJointVisibleInTree(const std::string& jointName);

    std::string& GetSelectedJointName() { return animEditorSelectedJointName_; }
    const std::string& GetSelectedJointName() const { return animEditorSelectedJointName_; }
    void SetSelectedJointName(const std::string& name) {
        animEditorSelectedJointName_ = name;
        animEditorSelectedKeyIndex_ = -1;
        EnsureJointVisibleInTree(name);
    }

    int& GetSelectedProperty() { return animEditorSelectedProperty_; }
    int GetSelectedProperty() const { return animEditorSelectedProperty_; }

    int& GetSelectedKeyIndex() { return animEditorSelectedKeyIndex_; }
    int GetSelectedKeyIndex() const { return animEditorSelectedKeyIndex_; }
    void SetSelectedKeyIndex(int idx) { animEditorSelectedKeyIndex_ = idx; }

    int& GetTargetAnim() { return animEditorTargetAnim_; }
    int GetTargetAnim() const { return animEditorTargetAnim_; }

    const std::vector<std::string>& GetCurrentJointList() const { return currentJointList_; }
    const std::vector<AnimJointTreeNode>& GetAnimJointTreeNodes() const { return animJointTreeNodes_; }
    const std::vector<int32_t>& GetAnimJointRootIndices() const { return animJointRootIndices_; }
    std::unordered_map<std::string, bool>& GetAnimJointExpanded() { return animJointExpanded_; }

    std::vector<std::string>& GetAvailableAnimationFiles() { return availableAnimationFiles_; }
    std::string& GetCurrentAnimFilePath() { return currentAnimFilePath_; }
    std::string GetCurrentModelName() const;
    std::string GetModelAnimationDirectory() const;

    std::unordered_map<std::string, TempBoneOverride>& GetTempOverrides() { return animTempOverrides_; }

    bool& GetIsAnimLocked() { return isAnimLocked_; }
    bool GetIsAnimLocked() const { return isAnimLocked_; }

    bool& GetIsAnimHudMinimized() { return isAnimHudMinimized_; }
    bool GetIsAnimHudMinimized() const { return isAnimHudMinimized_; }

    bool& GetAnimSymmetryMode() { return animSymmetryMode_; }
    bool GetAnimSymmetryMode() const { return animSymmetryMode_; }

    bool& GetAnimSymmetryAxisX() { return animSymmetryAxisX_; }
    bool& GetAnimSymmetryAxisY() { return animSymmetryAxisY_; }
    bool& GetAnimSymmetryAxisZ() { return animSymmetryAxisZ_; }

    std::map<std::string, std::string>& GetCustomSymmetryMap() { return customSymmetryMap_; }

    bool& GetIsAnimScenePushed() { return isAnimationScenePushed_; }
    bool IsAnimScenePushed() const { return isAnimationScenePushed_; }
    void SetAnimScenePushed(bool pushed) { isAnimationScenePushed_ = pushed; }

    bool& GetShowAnimEditor() { return showAnimEditor_; }
    bool GetShowAnimEditor() const { return showAnimEditor_; }
    void SetShowAnimEditor(bool show) { showAnimEditor_ = show; }

    bool& GetIsHovered() { return isHovered_; }
    bool IsHovered() const { return isHovered_; }
    void SetHovered(bool h) { isHovered_ = h; }

    AnimEditorSnapshot& GetAnimDragPreSnapshot() { return animDragPreSnapshot_; }
    bool& GetHasAnimDragPreSnapshot() { return hasAnimDragPreSnapshot_; }

    int& GetGizmoMode() { return animGizmoMode_; }
    int GetGizmoMode() const { return animGizmoMode_; }
    void SetGizmoMode(int mode) { animGizmoMode_ = mode; }

    int& GetGizmoSpace() { return animGizmoSpace_; }
    int GetGizmoSpace() const { return animGizmoSpace_; }
    void SetGizmoSpace(int space) { animGizmoSpace_ = space; }

    static constexpr size_t kMaxAnimNameBufSize = 256;
    char* GetNewAnimSaveNameBuf() { return newAnimSaveNameBuf_; }
    size_t GetNewAnimSaveNameBufSize() const { return sizeof(newAnimSaveNameBuf_); }
    bool& GetOpenSaveAnimModal() { return openSaveAnimModal_; }
    bool& GetOpenDeleteAnimModal() { return openDeleteAnimModal_; }

    bool& GetAnimEditorInitialized() { return animEditorInitialized_; }
    bool GetAnimEditorInitialized() const { return animEditorInitialized_; }
    void SetAnimEditorInitialized(bool init) { animEditorInitialized_ = init; }

private:
    Object3D* selectedObject_ = nullptr;
    std::shared_ptr<GameObject> selectedGameObject_ = nullptr;
    PrimitiveObject* selectedPrimitive_ = nullptr;

    bool isHovered_ = false;
    bool showAnimEditor_ = true;
    bool isAnimationScenePushed_ = false;

    int animEditorTargetAnim_ = 0;
    float animEditorTime_ = 0.0f;
    bool animEditorPlaying_ = false;
    AnimationWrapMode animEditorWrapMode_ = AnimationWrapMode::Loop;
    float animEditorFps_ = 60.0f;
    std::string animEditorSelectedJointName_ = "Hips_01";
    int animEditorSelectedProperty_ = 0;
    int animEditorSelectedKeyIndex_ = -1;

    Animation editingAnimation_;
    bool animEditorInitialized_ = false;
    std::vector<std::string> currentJointList_;
    std::vector<AnimJointTreeNode> animJointTreeNodes_;
    std::vector<int32_t> animJointRootIndices_;
    std::unordered_map<std::string, bool> animJointExpanded_;
    std::vector<std::string> availableAnimationFiles_;
    std::string currentAnimFilePath_ = "resources/json/shared/Player/wall_climb_animation.json";
    std::string lastTargetModelName_ = "";

    char newAnimSaveNameBuf_[kMaxAnimNameBufSize] = "";
    bool openSaveAnimModal_ = false;
    bool openDeleteAnimModal_ = false;

    std::unordered_map<std::string, TempBoneOverride> animTempOverrides_;

    int animGizmoMode_ = 1; // 0: Translate, 1: Rotate, 2: Scale
    int animGizmoSpace_ = 0; // 0: Local, 1: World

    bool isAnimLocked_ = false;
    bool isAnimHudMinimized_ = false;

    std::vector<AnimEditorSnapshot> animUndoStack_;
    std::vector<AnimEditorSnapshot> animRedoStack_;
    AnimEditorSnapshot animDragPreSnapshot_;
    bool hasAnimDragPreSnapshot_ = false;

    bool animSymmetryMode_ = true;
    bool animSymmetryAxisX_ = true;
    bool animSymmetryAxisY_ = false;
    bool animSymmetryAxisZ_ = false;
    std::map<std::string, std::string> customSymmetryMap_;
};
#endif
