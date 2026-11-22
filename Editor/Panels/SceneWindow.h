#pragma once
#include "IEditorPanel.h"
#include <cstdint>
#include <raylib.h>
#include <string>
#include <vector>

class SceneWindow : public IEditorPanel
{
public:
    SceneWindow() = default;
    ~SceneWindow() override = default;

    void Draw(GameEditor* editor) override;

private:
    enum class EGizmoMode
    {
        None,
        Move,
        AxisX,
        AxisY,
        AxisZ,
        Scale
    };

    static void DrawToolbarBackground();
    static bool s_bIconButton(
        std::string_view label, std::string_view icon, const struct ImVec2& size, std::string_view tooltip);
    static void s_fDrawSpinner(float radius, float thickness, const unsigned int& color);

    // One entry per selected object at the start of a drag. Recording before
    // and after values here keeps the whole gesture to a single undo command.
    struct FDragStart
    {
        uint32_t m_SceneId = 0;
        uint64_t m_EntityId = 0;
        Vector2 m_Position{ 0.0f, 0.0f };
        Vector3 m_Position3D{ 0.0f, 0.0f, 0.0f };
        Vector2 m_Scale{ 1.0f, 1.0f };
        float m_Rotation = 0.0f;
    };

    bool m_bDragging = false;
    bool m_bDraggingEntity = false;
    bool m_bPanning = false;
    uint32_t m_DragObjectId = 0;
    uint64_t m_DragEntityId = 0;
    Vector2 m_DragStartWorldMouse{};
    std::vector<FDragStart> m_DragStarts;
    std::vector<FDragStart> m_GizmoStarts;
    bool m_bGizmoGroupDrag = false;

    EGizmoMode m_GizmoMode = EGizmoMode::None;
    uint64_t m_GizmoObjectId = 0;
    bool m_bGizmoEntity = false;
    Vector2 m_GizmoStartPosition{};
    Vector2 m_GizmoStartWorldMouse{};
    Vector2 m_GizmoStartScale{ 1.0f, 1.0f };
    Vector2 m_GizmoStartCenter{};
    Vector2 m_GizmoBaseSize{ 32.0f, 32.0f };
    float m_GizmoStartDistance = 0.0f;
    bool m_bGizmo3D = false;
    Vector3 m_GizmoStartPosition3D{};
    Vector3 m_GizmoStartWorldMouse3D{};
    Vector2 m_GizmoStartScreenMouse3D{};
    float m_GizmoWorldPerPixel3D = 0.01f;
};
