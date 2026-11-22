#define IMGUI_DEFINE_MATH_OPERATORS
#include "SceneWindow.h"
#include "../../Engine/Camera2DMath.h"
#include "../../Engine/GameEngine.h"
#include "../../Engine/MapManager.h"
#include "../../Engine/ProjectManager.h"
#include "../EditorPreferences.h"
#include "../GameEditor.h"
#include "../SceneCommands.h"
#include "ProcessRunner.h"

#include "../../Engine/Profiler.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <limits>
#include <memory>
#include <numbers>
#include <raymath.h>
#include <rlImGui.h>
#include <tinyfiledialogs.h>
#include <vector>

namespace
{
Vector2 RotatePoint(Vector2 point, float degrees)
{
    const float radians = degrees * DEG2RAD;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return { (point.x * cosine) - (point.y * sine), (point.x * sine) + (point.y * cosine) };
}

float DistanceToSegment(ImVec2 point, ImVec2 start, ImVec2 end)
{
    const ImVec2 segment{ end.x - start.x, end.y - start.y };
    const float length_squared = (segment.x * segment.x) + (segment.y * segment.y);
    if (length_squared <= 0.0f)
    {
        return std::sqrt(((point.x - start.x) * (point.x - start.x)) + ((point.y - start.y) * (point.y - start.y)));
    }

    const float t = std::clamp(
        (((point.x - start.x) * segment.x) + ((point.y - start.y) * segment.y)) / length_squared, 0.0f, 1.0f);
    const ImVec2 closest{ start.x + segment.x * t, start.y + segment.y * t };
    return std::sqrt(((point.x - closest.x) * (point.x - closest.x)) + ((point.y - closest.y) * (point.y - closest.y)));
}

struct FGizmo2D
{
    ImVec2 center{};
    ImVec2 axis_x_end{};
    ImVec2 axis_y_end{};
};

FGizmo2D MakeGizmo2D(ImVec2 center)
{
    return { center, { center.x + 56.0f, center.y }, { center.x, center.y - 56.0f } };
}

// Shared axis handles for scene sprites and gameplay entities (translate only).
void DrawGizmo2D(ImDrawList* draw_list, const FGizmo2D& gizmo, bool b_active_x, bool b_active_y)
{
    const ImU32 axis_x_color = IM_COL32(235, 90, 90, 220);
    const ImU32 axis_y_color = IM_COL32(100, 210, 130, 220);
    const ImU32 handle_fill = IM_COL32(245, 245, 245, 235);
    const ImU32 handle_edge = IM_COL32(20, 20, 20, 220);

    if (b_active_y)
    {
        draw_list->AddLine(gizmo.center, gizmo.axis_x_end, IM_COL32(235, 90, 90, 70), 1.5f);
    }
    if (b_active_x)
    {
        draw_list->AddLine(gizmo.center, gizmo.axis_y_end, IM_COL32(100, 210, 130, 70), 1.5f);
    }

    draw_list->AddLine(gizmo.center, gizmo.axis_x_end, axis_x_color, 2.0f);
    draw_list->AddTriangleFilled(gizmo.axis_x_end, ImVec2(gizmo.axis_x_end.x - 9.0f, gizmo.axis_x_end.y - 5.0f),
        ImVec2(gizmo.axis_x_end.x - 9.0f, gizmo.axis_x_end.y + 5.0f), axis_x_color);

    draw_list->AddLine(gizmo.center, gizmo.axis_y_end, axis_y_color, 2.0f);
    draw_list->AddTriangleFilled(gizmo.axis_y_end, ImVec2(gizmo.axis_y_end.x - 5.0f, gizmo.axis_y_end.y + 9.0f),
        ImVec2(gizmo.axis_y_end.x + 5.0f, gizmo.axis_y_end.y + 9.0f), axis_y_color);

    const ImVec2 square_min{ gizmo.center.x - 4.0f, gizmo.center.y - 4.0f };
    const ImVec2 square_max{ gizmo.center.x + 4.0f, gizmo.center.y + 4.0f };
    draw_list->AddRectFilled(square_min, square_max, handle_fill);
    draw_list->AddRect(square_min, square_max, handle_edge);
}

Matrix CameraProjectionMatrix(const Camera3D& camera, float aspect)
{
    if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        const float top = camera.fovy * 0.5f;
        const float right = top * aspect;
        return MatrixOrtho(-right, right, -top, top, 0.05f, 1000.0f);
    }
    return MatrixPerspective(camera.fovy * DEG2RAD, aspect, 0.05f, 1000.0f);
}

bool ProjectWorldToScene(
    Vector3 world, const Camera3D& camera, float scene_width, float scene_height, ImVec2& out_screen)
{
    const Matrix view = GetCameraMatrix(camera);
    const Matrix projection = CameraProjectionMatrix(camera, scene_width / std::max(scene_height, 1.0f));
    const Matrix view_projection = MatrixMultiply(view, projection);

    const float x = (view_projection.m0 * world.x) + (view_projection.m4 * world.y) + (view_projection.m8 * world.z) +
        view_projection.m12;
    const float y = (view_projection.m1 * world.x) + (view_projection.m5 * world.y) + (view_projection.m9 * world.z) +
        view_projection.m13;
    const float w = (view_projection.m3 * world.x) + (view_projection.m7 * world.y) + (view_projection.m11 * world.z) +
        view_projection.m15;

    if (w <= 0.0001f)
    {
        return false;
    }

    const float ndc_x = x / w;
    const float ndc_y = y / w;
    out_screen = { (ndc_x + 1.0f) * 0.5f * scene_width, (1.0f - ndc_y) * 0.5f * scene_height };
    return true;
}

bool UnprojectSceneToPlane(
    ImVec2 scene_point, const Camera3D& camera, float scene_width, float scene_height, float plane_y, Vector3& out)
{
    const Matrix view = GetCameraMatrix(camera);
    const Matrix projection = CameraProjectionMatrix(camera, scene_width / std::max(scene_height, 1.0f));

    const float ndc_x = ((scene_point.x / std::max(scene_width, 1.0f)) * 2.0f) - 1.0f;
    const float ndc_y = 1.0f - ((scene_point.y / std::max(scene_height, 1.0f)) * 2.0f);
    const Vector3 near_point = Vector3Unproject({ ndc_x, ndc_y, -1.0f }, projection, view);
    const Vector3 far_point = Vector3Unproject({ ndc_x, ndc_y, 1.0f }, projection, view);
    const Vector3 direction = Vector3Normalize(Vector3Subtract(far_point, near_point));

    if (std::fabs(direction.y) <= 0.0001f)
    {
        return false;
    }

    const float distance = (plane_y - near_point.y) / direction.y;
    if (distance <= 0.0f)
    {
        return false;
    }

    out = Vector3Add(near_point, Vector3Scale(direction, distance));
    return true;
}

// World-space reference grid for the stopped 2D viewport. Two line levels:
// minor lines every `grid_size`, brighter lines every fourth step. The stride
// is promoted when zoomed far out so a dense grid never floods the draw list.
template <typename FWorldToScreen>
void DrawWorldGrid(ImDrawList* draw_list, ImVec2 clip_min, ImVec2 clip_max, Vector2 world_min, Vector2 world_max,
    float grid_size, FWorldToScreen world_to_screen)
{
    if (grid_size < 1.0f)
    {
        return;
    }

    constexpr float c_MAX_LINES = 256.0f;

    float step = grid_size;
    if (((world_max.x - world_min.x) / step) > c_MAX_LINES || ((world_max.y - world_min.y) / step) > c_MAX_LINES)
    {
        step *= 4.0f;
    }
    if (((world_max.x - world_min.x) / step) > c_MAX_LINES || ((world_max.y - world_min.y) / step) > c_MAX_LINES)
    {
        return;
    }

    // Bounds the float->int grid indices: scenes live near the origin, and a
    // runaway pan should drop the grid rather than overflow the index.
    constexpr float c_MAX_ABS_COORD = 1.0e7f;
    if (std::fabs(world_min.x) > c_MAX_ABS_COORD || std::fabs(world_max.x) > c_MAX_ABS_COORD ||
        std::fabs(world_min.y) > c_MAX_ABS_COORD || std::fabs(world_max.y) > c_MAX_ABS_COORD)
    {
        return;
    }

    const int first_x = static_cast<int>(std::floor(world_min.x / step));
    const int last_x = static_cast<int>(std::ceil(world_max.x / step));
    const int first_y = static_cast<int>(std::floor(world_min.y / step));
    const int last_y = static_cast<int>(std::ceil(world_max.y / step));

    const ImU32 minor_color = IM_COL32(255, 255, 255, 18);
    const ImU32 major_color = IM_COL32(255, 255, 255, 46);

    draw_list->PushClipRect(clip_min, clip_max, true);

    for (int index = first_x; index <= last_x; ++index)
    {
        const float world_x = static_cast<float>(index) * step;
        const bool b_major = (index % 4) == 0;
        draw_list->AddLine(world_to_screen({ world_x, world_min.y }), world_to_screen({ world_x, world_max.y }),
            b_major ? major_color : minor_color, 1.0f);
    }
    for (int index = first_y; index <= last_y; ++index)
    {
        const float world_y = static_cast<float>(index) * step;
        const bool b_major = (index % 4) == 0;
        draw_list->AddLine(world_to_screen({ world_min.x, world_y }), world_to_screen({ world_max.x, world_y }),
            b_major ? major_color : minor_color, 1.0f);
    }

    draw_list->PopClipRect();
}
} // namespace

void SceneWindow::DrawToolbarBackground()
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 toolbar_pos = ImGui::GetCursorScreenPos();
    ImVec2 toolbar_size = ImVec2(ImGui::GetContentRegionAvail().x, 40.0f);

    ImU32 toolbar_color_top = ImGui::GetColorU32(ImGuiCol_MenuBarBg);
    ImU32 toolbar_color_bottom = ImGui::GetColorU32(ImGuiCol_WindowBg);

    draw_list->AddRectFilledMultiColor(toolbar_pos,
        ImVec2(toolbar_pos.x + toolbar_size.x, toolbar_pos.y + toolbar_size.y), toolbar_color_top, toolbar_color_top,
        toolbar_color_bottom, toolbar_color_bottom);
}

void SceneWindow::s_fDrawSpinner(float radius, float thickness, const unsigned int& color)
{
    alignas(16) struct
    {
        float time;
        float start;
        float a_min;
        float a_max;
        float centre_x;
        float centre_y;
        float time_x8;
        float inv_num_segments;
        float angle_range;
    } vars{};

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(radius * 2, radius * 2));
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    DrawList->PathClear();

    vars.time = static_cast<float>(ImGui::GetTime());
    constexpr int c_NUM_SEGMENTS = 30;
    vars.start = fabsf(sinf(vars.time * 1.8f) * (c_NUM_SEGMENTS - 5));

    vars.inv_num_segments = 1.0f / static_cast<float>(c_NUM_SEGMENTS);
    vars.time_x8 = vars.time * 8.0f;

    vars.a_min = std::numbers::pi_v<float> * 2.0f * vars.start * vars.inv_num_segments;
    vars.a_max = std::numbers::pi_v<float> * 2.0f * static_cast<float>(c_NUM_SEGMENTS - 3) * vars.inv_num_segments;
    vars.angle_range = vars.a_max - vars.a_min;

    const ImVec2 CENTER = ImVec2(pos.x + radius, pos.y + radius);
    vars.centre_x = CENTER.x;
    vars.centre_y = CENTER.y;

    for (int i = 0; i < c_NUM_SEGMENTS; ++i)
    {
        const float A = vars.a_min + ((static_cast<float>(i) * vars.inv_num_segments) * vars.angle_range);
        const float ANGLE = A + vars.time_x8;
        DrawList->PathLineTo(ImVec2(vars.centre_x + (cosf(ANGLE) * radius), vars.centre_y + (sinf(ANGLE) * radius)));
    }
    DrawList->PathStroke(color, 0, thickness);
}

bool SceneWindow::s_bIconButton(
    std::string_view label, std::string_view icon, const ImVec2& size, std::string_view tooltip)
{
    ImGui::PushID(label.data());
    bool b_Clicked = ImGui::Button(icon.data(), size);

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
    }
    ImGui::PopID();
    return b_Clicked;
}

void SceneWindow::Draw(GameEditor* editor)
{
    SCOPED_TIMER("panel_scene_window");
    t_EditorPreferences& prefs = EditorPreferences::GetInstance().GetPreferences();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMenuButtonPosition = ImGuiDir_Left;

    ImGui::Begin(ICON_FA_IMAGE " Viewport", nullptr, ImGuiWindowFlags_NoTitleBar);

    ImGuiDockNode* node = ImGui::DockBuilderGetNode(0x00000001);
    if (node != nullptr)
    {
        node->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
    }

    DrawToolbarBackground();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

    float toolbar_height = 40.0f;
    float button_height = 32.0f;
    float vertical_offset = (toolbar_height - button_height) / 2.0f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + vertical_offset);
    const float toolbar_button_y = ImGui::GetCursorPosY();

    const GameEditor::EPlayState play_state = editor->GetPlayState();

    std::string_view primary_icon = ICON_FA_PLAY;
    std::string_view primary_tooltip = "Play";
    std::string_view status_icon = ICON_FA_STOP;
    std::string_view status_label = " STOPPED";
    ImVec4 status_color(0.7f, 0.15f, 0.15f, 1.0f);
    if (play_state == GameEditor::EPlayState::Playing)
    {
        primary_icon = ICON_FA_PAUSE;
        primary_tooltip = "Pause";
        status_icon = ICON_FA_PLAY;
        status_label = " PLAYING";
        status_color = ImVec4(0.15f, 0.6f, 0.15f, 1.0f);
    }
    else if (play_state == GameEditor::EPlayState::Paused)
    {
        primary_icon = ICON_FA_PLAY;
        primary_tooltip = "Resume";
        status_icon = ICON_FA_PAUSE;
        status_label = " PAUSED";
        status_color = ImVec4(0.65f, 0.45f, 0.1f, 1.0f);
    }

    const std::string status_full = std::string(status_icon) + std::string(status_label);
    const float status_w = ImGui::CalcTextSize(status_full.c_str()).x + 32.0f;
    const float spacing_x = ImGui::GetStyle().ItemSpacing.x;
    const float total_center_w = (32.0f * 8.0f) + status_w + (8.0f * spacing_x) + 24.0f;

    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_center_w) * 0.5f);

    // Play / Pause / Resume
    if (s_bIconButton("play_pause_btn", primary_icon, ImVec2(32, 32), primary_tooltip))
    {
        switch (play_state)
        {
            case GameEditor::EPlayState::Stopped:
                editor->SetPlayState(GameEditor::EPlayState::Playing);
                break;
            case GameEditor::EPlayState::Playing:
                editor->SetPlayState(GameEditor::EPlayState::Paused);
                break;
            case GameEditor::EPlayState::Paused:
                editor->SetPlayState(GameEditor::EPlayState::Playing);
                break;
        }
    }

    ImGui::SameLine();

    // Stop returns to the editor camera. Disabled while already stopped.
    {
        const bool b_can_stop = play_state != GameEditor::EPlayState::Stopped;
        if (!b_can_stop)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        }
        if (s_bIconButton("stop_btn", ICON_FA_STOP, ImVec2(32, 32), "Stop") && b_can_stop)
        {
            editor->SetPlayState(GameEditor::EPlayState::Stopped);
        }
        if (!b_can_stop)
        {
            ImGui::PopStyleVar();
        }
    }

    ImGui::SameLine();

    // Step advances exactly one fixed 1/60s frame; only meaningful while paused.
    {
        const bool b_can_step = play_state == GameEditor::EPlayState::Paused;
        if (!b_can_step)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        }
        if (s_bIconButton("step_btn", ICON_FA_FORWARD_STEP, ImVec2(32, 32), "Step (paused only)") && b_can_step)
        {
            editor->StepOnce();
        }
        if (!b_can_step)
        {
            ImGui::PopStyleVar();
        }
    }

    ImGui::SameLine();

    // Restart reloads through MapManager. Plain GameMap projects have none,
    // so the button is disabled there rather than doing nothing on click.
    MapManager* restart_map_manager = editor->GetMapManager();
    const bool b_CanRestart = restart_map_manager != nullptr;
    if (!b_CanRestart)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    if (s_bIconButton("restart_btn", ICON_FA_ARROW_ROTATE_RIGHT, ImVec2(32, 32),
            b_CanRestart ? "Restart" : "Restart unavailable: project has no MapManager") &&
        b_CanRestart)
    {
        editor->SetPlayState(GameEditor::EPlayState::Stopped);
        editor->InvalidateEditorEntities();
        restart_map_manager->b_ReloadCurrentMap();
    }
    if (!b_CanRestart)
        ImGui::PopStyleVar();

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12);

    // Status pill
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, status_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, status_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, status_color);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    ImGui::Button(status_full.c_str(), ImVec2(0, 32.0f));

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f);

    // Restore
    if (s_bIconButton("restore_btn", ICON_FA_ARROW_ROTATE_LEFT, ImVec2(32, 32), "Reset Game"))
    {
        editor->SetPlayState(GameEditor::EPlayState::Stopped);
        if (!editor->b_ReloadGameLogic())
            editor->GetGameEngine().ResetMap();
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);

    // Clean
    if (s_bIconButton("clean_btn", ICON_FA_TRASH_CAN, ImVec2(32, 32), "Delete Build Folder"))
    {
        if (ProjectManager::b_HasOpenProject())
        {
            auto raywaves_build =
                std::filesystem::path(ProjectManager::GetCurrent().m_RootPath) / ".raywaves" / "build";
            if (std::filesystem::exists(raywaves_build))
                std::filesystem::remove_all(raywaves_build);
        }
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5);

    // Performance Toggle
    const ImVec4 STATS_COLOR = editor->m_bShowPerformanceStats ? ImGui::GetStyle().Colors[ImGuiCol_Text]
                                                               : ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
    ImGui::PushStyleColor(ImGuiCol_Text, STATS_COLOR);

    if (s_bIconButton("perf_btn", ICON_FA_CHART_LINE, ImVec2(32, 32), "Performance Overlay"))
        editor->m_bShowPerformanceStats = !editor->m_bShowPerformanceStats;
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // Terminal Toggle
    const ImVec4 TERM_COLOR = editor->m_bShowTerminal ? ImGui::GetStyle().Colors[ImGuiCol_Text]
                                                      : ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
    ImGui::PushStyleColor(ImGuiCol_Text, TERM_COLOR);
    if (s_bIconButton("term_btn", ICON_FA_TERMINAL, ImVec2(32, 32), "Terminal"))
        editor->m_bShowTerminal = !editor->m_bShowTerminal;
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // Compile
    const float button_sz = 32.0f + (ImGui::GetStyle().FramePadding.x * 2.0f);
    float status_sz = 0.0f;
    std::string compile_label;
    if (editor->b_IsCompiling)
    {
        compile_label = "Compiling " + prefs.BuildConfig + "...";
        status_sz = 20.0f + (3.0f * spacing_x) + ImGui::CalcTextSize(compile_label.c_str()).x + 32.0f;
    }

    const float avail = ImGui::GetContentRegionAvail().x;
    const float pos_x = ImGui::GetCursorPosX() + avail - button_sz - status_sz;

    if (pos_x > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(pos_x);

    if (editor->b_IsCompiling)
    {
        ImGui::SetCursorPosY(toolbar_button_y + ((button_height - 20.0f) * 0.5f));
        s_fDrawSpinner(10.0f, 2.0f, ImGui::GetColorU32(ImVec4(0.2f, 0.8f, 0.2f, 1.0f)));

        ImGui::SameLine();
        ImGui::SetCursorPosY(toolbar_button_y + ((button_height - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s", compile_label.c_str());

        ImGui::SameLine();
        ImGui::SetCursorPosY(toolbar_button_y);
        if (s_bIconButton("cancel_build_btn", ICON_FA_XMARK, ImVec2(32, 32), "Cancel Build"))
            editor->CancelBuild();

        ImGui::SameLine();
        ImGui::SetCursorPosY(toolbar_button_y);
    }

    const bool b_Disabled = editor->b_IsCompiling.load();
    if (b_Disabled)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);

    if (s_bIconButton("compile_btn", ICON_FA_HAMMER, ImVec2(32, 32), "Recompile"))
    {
        if (!b_Disabled)
        {
            editor->CompileGameLogic();
        }
    }

    if (b_Disabled)
        ImGui::PopStyleVar();
    ImGui::PopStyleVar(3);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild(
        "ViewportArea", ImVec2(0, 0), 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    RenderTexture2D* display = editor->m_bUseOpaquePass ? &editor->m_DisplayTexture : &editor->m_RaylibTexture;
    const Texture2D texture = display->texture;

    if (texture.id != 0)
    {
        const ImVec2 area = ImGui::GetContentRegionAvail();
        float image_scale =
            std::min(area.x / static_cast<float>(texture.width), area.y / static_cast<float>(texture.height));
        if (image_scale <= 0.0f)
        {
            image_scale = 1.0f;
        }

        const ImVec2 image_size{ static_cast<float>(texture.width) * image_scale,
            static_cast<float>(texture.height) * image_scale };
        ImVec2 image_min = ImGui::GetCursorScreenPos();
        image_min.x += (area.x - image_size.x) * 0.5f;
        image_min.y += (area.y - image_size.y) * 0.5f;
        const ImVec2 image_max{ image_min.x + image_size.x, image_min.y + image_size.y };

        ImGui::SetCursorScreenPos(image_min);
        ImGui::Image(static_cast<ImTextureID>(texture.id), image_size, ImVec2(0.0f, 0.0f), ImVec2(1.0f, -1.0f));

        const bool b_image_hovered = ImGui::IsItemHovered();
        Scene& scene = editor->GetScene();
        const float scene_scale = image_size.x / static_cast<float>(editor->m_SceneSettings.m_SceneWidth);

        GameMap* active_map = editor->GetActiveMap();
        const Camera2D view_camera = (active_map != nullptr) ? active_map->GetViewCamera()
                                                             : Camera2D{ { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f, 1.0f };
        Camera2D* mutable_camera = editor->GetMutableActiveViewCamera();

        auto screen_to_scene = [image_min, scene_scale](ImVec2 point) -> Vector2
        {
            if (scene_scale <= 0.0f)
            {
                return { 0.0f, 0.0f };
            }
            return { (point.x - image_min.x) / scene_scale, (point.y - image_min.y) / scene_scale };
        };

        auto scene_to_world = [view_camera](Vector2 scene_point) -> Vector2
        { return GetScreenToWorld2D(scene_point, view_camera); };

        auto world_to_scene = [view_camera](Vector2 world_point) -> Vector2
        { return GetWorldToScreen2D(world_point, view_camera); };

        auto object_rect = [](const t_SceneObject& object) -> Rectangle
        {
            float width = 32.0f;
            float height = 32.0f;
            if (IsTextureValid(object.m_Texture))
            {
                if (object.m_Source.width > 0.0f && object.m_Source.height > 0.0f)
                {
                    width = object.m_Source.width;
                    height = object.m_Source.height;
                }
                else
                {
                    width = static_cast<float>(object.m_Texture.width);
                    height = static_cast<float>(object.m_Texture.height);
                }
            }
            return { object.m_Position.x, object.m_Position.y, width * object.m_Scale.x, height * object.m_Scale.y };
        };

        auto entity_rect = [](const GameEditor::FEditorEntityRef& entity) -> Rectangle
        {
            const float width = (entity.m_Size != nullptr) ? entity.m_Size->x : 32.0f;
            const float height = (entity.m_Size != nullptr) ? entity.m_Size->y : 32.0f;
            return { entity.m_Position->x + entity.m_Origin.x, entity.m_Position->y + entity.m_Origin.y, width,
                height };
        };

        auto object_base_size = [](const t_SceneObject& object) -> Vector2
        {
            if (!IsTextureValid(object.m_Texture))
            {
                return { 32.0f, 32.0f };
            }
            if (object.m_Source.width > 0.0f && object.m_Source.height > 0.0f)
            {
                return { object.m_Source.width, object.m_Source.height };
            }
            return { static_cast<float>(object.m_Texture.width), static_cast<float>(object.m_Texture.height) };
        };

        auto scene_to_screen = [image_min, scene_scale](Vector2 scene_point) -> ImVec2
        { return { image_min.x + (scene_point.x * scene_scale), image_min.y + (scene_point.y * scene_scale) }; };

        auto world_to_screen = [&scene_to_screen, &world_to_scene](Vector2 world_point) -> ImVec2
        { return scene_to_screen(world_to_scene(world_point)); };

        // Reference grid sits on the world origin and is drawn after the scene
        // texture but before selection outlines and handles. 3D maps have no 2D
        // world plane, so the grid only applies to the 2D view.
        if (!editor->b_IsPlaying && prefs.SceneGridEnabled && !editor->b_ActiveMapUses3DView())
        {
            const Vector2 world_a = scene_to_world(screen_to_scene(image_min));
            const Vector2 world_b = scene_to_world(screen_to_scene(image_max));
            const Vector2 world_min{ std::min(world_a.x, world_b.x), std::min(world_a.y, world_b.y) };
            const Vector2 world_max{ std::max(world_a.x, world_b.x), std::max(world_a.y, world_b.y) };

            DrawWorldGrid(ImGui::GetWindowDrawList(), image_min, image_max, world_min, world_max, prefs.SceneGridSize,
                [&world_to_screen](Vector2 world_point) { return world_to_screen(world_point); });
        }

        // Selected outlines. Drawn even while playing so the selection stays
        // visible, but editing handles are stopped-only.
        ImVec2 object_corners[4]{};
        Vector2 object_center_world{};
        Vector2 object_base{ 32.0f, 32.0f };
        bool b_has_object_selection = false;

        const auto draw_scene_object_outline = [&](const t_SceneObject& selected, bool b_primary)
        {
            const Vector2 base = object_base_size(selected);
            const Vector2 local_corners[4] = {
                { 0.0f, 0.0f },
                { base.x * selected.m_Scale.x, 0.0f },
                { base.x * selected.m_Scale.x, base.y * selected.m_Scale.y },
                { 0.0f, base.y * selected.m_Scale.y },
            };

            ImVec2 corners[4];
            for (int index = 0; index < 4; ++index)
            {
                const Vector2 rotated = RotatePoint(local_corners[index], selected.m_Rotation);
                corners[index] = world_to_screen({
                    selected.m_Position.x + rotated.x,
                    selected.m_Position.y + rotated.y,
                });
            }

            ImGui::GetWindowDrawList()->AddPolyline(
                corners, 4, IM_COL32(120, 200, 255, 235), ImDrawFlags_Closed, b_primary ? 2.0f : 1.0f);

            if (b_primary)
            {
                object_base = base;
                for (int index = 0; index < 4; ++index)
                {
                    object_corners[index] = corners[index];
                }

                const Vector2 half = { base.x * selected.m_Scale.x * 0.5f, base.y * selected.m_Scale.y * 0.5f };
                const Vector2 rotated_center = RotatePoint(half, selected.m_Rotation);
                object_center_world = {
                    selected.m_Position.x + rotated_center.x,
                    selected.m_Position.y + rotated_center.y,
                };
            }
        };

        if (editor->m_SelectionKind == GameEditor::EEditorSelectionKind::GameplayEntity)
        {
            for (const uint64_t id : editor->GetSelectedEditorEntityIds())
            {
                for (const GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
                {
                    if (entity.m_Id != id || entity.m_Position == nullptr)
                    {
                        continue;
                    }

                    const Rectangle rect = entity_rect(entity);
                    const ImVec2 min = world_to_screen({ rect.x, rect.y });
                    const ImVec2 max = world_to_screen({ rect.x + rect.width, rect.y + rect.height });
                    ImGui::GetWindowDrawList()->AddRect(min, max, IM_COL32(120, 200, 255, 235), 0.0f, 0, 1.5f);
                    break;
                }
            }
        }
        else if (editor->m_SelectionKind == GameEditor::EEditorSelectionKind::SceneObject)
        {
            // Draw the primary last so its thicker outline stays on top.
            for (const uint32_t id : editor->GetSelectedSceneObjectIds())
            {
                const bool b_primary = id == editor->m_SelectedSceneObjectId;
                if (b_primary)
                {
                    continue;
                }
                if (const t_SceneObject* selected = scene.FindObject(id))
                {
                    draw_scene_object_outline(*selected, false);
                }
            }

            if (const t_SceneObject* primary = scene.FindObject(editor->m_SelectedSceneObjectId))
            {
                draw_scene_object_outline(*primary, true);
                b_has_object_selection = true;
            }
        }

        bool b_gizmo_consumed = false;
        bool b_camera_navigating = false;
        if (!editor->b_IsPlaying && b_has_object_selection)
        {
            t_SceneObject* selected = editor->GetSelectedSceneObject();
            if (selected != nullptr)
            {
                if ((m_GizmoMode != EGizmoMode::None) && (m_bGizmoEntity || (m_GizmoObjectId != selected->m_Id)))
                {
                    m_GizmoMode = EGizmoMode::None;
                }

                const ImVec2 center = world_to_screen(object_center_world);
                const ImVec2 axis_x_end{ center.x + 56.0f, center.y };
                const ImVec2 axis_y_end{ center.x, center.y - 56.0f };
                const float handle_radius = 7.0f;

                auto hover_circle = [&](ImVec2 point) -> bool
                {
                    const ImVec2 mouse = ImGui::GetIO().MousePos;
                    return ((mouse.x - point.x) * (mouse.x - point.x)) + ((mouse.y - point.y) * (mouse.y - point.y)) <=
                        (handle_radius * handle_radius);
                };
                auto hover_rect = [&](ImVec2 point) -> bool
                {
                    const ImVec2 mouse = ImGui::GetIO().MousePos;
                    return (mouse.x >= point.x - handle_radius) && (mouse.x <= point.x + handle_radius) &&
                        (mouse.y >= point.y - handle_radius) && (mouse.y <= point.y + handle_radius);
                };

                int hovered_corner = -1;
                for (int index = 0; index < 4; ++index)
                {
                    if (hover_rect(object_corners[index]))
                    {
                        hovered_corner = index;
                        break;
                    }
                }

                const bool b_hover_center = hover_circle(center);
                const bool b_hover_x = DistanceToSegment(ImGui::GetIO().MousePos, center, axis_x_end) <= handle_radius;
                const bool b_hover_y = DistanceToSegment(ImGui::GetIO().MousePos, center, axis_y_end) <= handle_radius;

                if ((hovered_corner >= 0) || b_hover_center || b_hover_x || b_hover_y)
                {
                    ImGui::SetMouseCursor((hovered_corner >= 0) ? ImGuiMouseCursor_ResizeNWSE : ImGuiMouseCursor_Hand);
                }

                if ((m_GizmoMode == EGizmoMode::None) && b_image_hovered &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    if (hovered_corner >= 0)
                    {
                        m_GizmoMode = EGizmoMode::Scale;
                    }
                    else if (b_hover_center)
                    {
                        m_GizmoMode = EGizmoMode::Move;
                    }
                    else if (b_hover_x)
                    {
                        m_GizmoMode = EGizmoMode::AxisX;
                    }
                    else if (b_hover_y)
                    {
                        m_GizmoMode = EGizmoMode::AxisY;
                    }

                    if (m_GizmoMode != EGizmoMode::None)
                    {
                        m_bGizmo3D = false;
                        m_bGizmoEntity = false;
                        m_GizmoObjectId = selected->m_Id;
                        m_GizmoStartPosition = selected->m_Position;
                        m_GizmoStartScale = selected->m_Scale;
                        m_GizmoStartCenter = object_center_world;
                        m_GizmoBaseSize = object_base;
                        m_GizmoStartWorldMouse = scene_to_world(screen_to_scene(ImGui::GetIO().MousePos));
                        m_GizmoStartDistance = std::sqrt(
                            ((ImGui::GetIO().MousePos.x - center.x) * (ImGui::GetIO().MousePos.x - center.x)) +
                            ((ImGui::GetIO().MousePos.y - center.y) * (ImGui::GetIO().MousePos.y - center.y)));
                        m_GizmoStartDistance = std::max(m_GizmoStartDistance, 1.0f);

                        m_GizmoStarts.clear();
                        m_bGizmoGroupDrag = m_GizmoMode != EGizmoMode::Scale;
                        m_GizmoStarts.push_back(
                            { selected->m_Id, 0, selected->m_Position, {}, selected->m_Scale, selected->m_Rotation });
                        if (m_bGizmoGroupDrag)
                        {
                            for (const uint32_t id : editor->GetSelectedSceneObjectIds())
                            {
                                if (id == selected->m_Id)
                                {
                                    continue;
                                }
                                if (const t_SceneObject* object = scene.FindObject(id))
                                {
                                    m_GizmoStarts.push_back(
                                        { id, 0, object->m_Position, {}, object->m_Scale, object->m_Rotation });
                                }
                            }
                        }
                        b_gizmo_consumed = true;
                    }
                }

                if ((m_GizmoMode != EGizmoMode::None) && !m_bGizmoEntity && (m_GizmoObjectId == selected->m_Id))
                {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    {
                        const Vector2 world_mouse = scene_to_world(screen_to_scene(ImGui::GetIO().MousePos));
                        Vector2 next_position = selected->m_Position;

                        switch (m_GizmoMode)
                        {
                            case EGizmoMode::Move:
                                next_position = { m_GizmoStartPosition.x + (world_mouse.x - m_GizmoStartWorldMouse.x),
                                    m_GizmoStartPosition.y + (world_mouse.y - m_GizmoStartWorldMouse.y) };
                                break;
                            case EGizmoMode::AxisX:
                                next_position.x = m_GizmoStartPosition.x + (world_mouse.x - m_GizmoStartWorldMouse.x);
                                break;
                            case EGizmoMode::AxisY:
                                next_position.y = m_GizmoStartPosition.y + (world_mouse.y - m_GizmoStartWorldMouse.y);
                                break;
                            case EGizmoMode::Scale:
                            {
                                const ImVec2 center_screen = world_to_screen(m_GizmoStartCenter);
                                const float distance = std::sqrt(((ImGui::GetIO().MousePos.x - center_screen.x) *
                                                                     (ImGui::GetIO().MousePos.x - center_screen.x)) +
                                    ((ImGui::GetIO().MousePos.y - center_screen.y) *
                                        (ImGui::GetIO().MousePos.y - center_screen.y)));
                                const float factor = std::clamp(distance / m_GizmoStartDistance, 0.05f, 20.0f);
                                const Vector2 next_scale = { m_GizmoStartScale.x * factor,
                                    m_GizmoStartScale.y * factor };
                                const Vector2 half = { m_GizmoBaseSize.x * next_scale.x * 0.5f,
                                    m_GizmoBaseSize.y * next_scale.y * 0.5f };
                                const Vector2 offset = RotatePoint(half, selected->m_Rotation);

                                selected->m_Scale = next_scale;
                                selected->m_Position = { m_GizmoStartCenter.x - offset.x,
                                    m_GizmoStartCenter.y - offset.y };
                                scene.MarkDirty();
                                b_gizmo_consumed = true;
                                break;
                            }
                            default:
                                break;
                        }

                        if (m_GizmoMode != EGizmoMode::Scale)
                        {
                            const Vector2 delta = { next_position.x - m_GizmoStartPosition.x,
                                next_position.y - m_GizmoStartPosition.y };
                            for (const FDragStart& start : m_GizmoStarts)
                            {
                                t_SceneObject* object = scene.FindObject(start.m_SceneId);
                                if (object == nullptr)
                                {
                                    continue;
                                }

                                Vector2 position = { start.m_Position.x + delta.x, start.m_Position.y + delta.y };
                                if (ImGui::GetIO().KeyCtrl)
                                {
                                    constexpr float c_SNAP = 8.0f;
                                    position.x = std::round(position.x / c_SNAP) * c_SNAP;
                                    position.y = std::round(position.y / c_SNAP) * c_SNAP;
                                }
                                object->m_Position = position;
                            }
                            scene.MarkDirty();
                            b_gizmo_consumed = true;
                        }
                    }
                    else
                    {
                        std::vector<SceneCommands::FSceneObjectTransformEntry> entries;
                        for (const FDragStart& start : m_GizmoStarts)
                        {
                            t_SceneObject* object = scene.FindObject(start.m_SceneId);
                            if (object == nullptr)
                            {
                                continue;
                            }

                            const SceneCommands::FSceneObjectTransform after = SceneCommands::ReadTransform(*object);
                            if (start.m_Position.x == after.m_Position.x && start.m_Position.y == after.m_Position.y &&
                                start.m_Rotation == after.m_Rotation && start.m_Scale.x == after.m_Scale.x &&
                                start.m_Scale.y == after.m_Scale.y)
                            {
                                continue;
                            }

                            SceneCommands::FSceneObjectTransformEntry entry;
                            entry.m_Id = start.m_SceneId;
                            entry.m_Before = { start.m_Position, start.m_Rotation, start.m_Scale };
                            entry.m_After = after;
                            entries.push_back(entry);
                        }
                        if (!entries.empty())
                        {
                            editor->GetCommandStack().Record(
                                std::make_unique<SceneCommands::SceneObjectsTransformCommand>(
                                    scene, std::move(entries), "Transform Object"));
                        }
                        m_GizmoStarts.clear();
                        m_bGizmoGroupDrag = false;
                        m_GizmoMode = EGizmoMode::None;
                    }
                }

                // Draw handles after applying input so they track the object.
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                const ImU32 axis_x_color = IM_COL32(235, 90, 90, 220);
                const ImU32 axis_y_color = IM_COL32(100, 210, 130, 220);
                const ImU32 handle_fill = IM_COL32(245, 245, 245, 235);
                const ImU32 handle_edge = IM_COL32(20, 20, 20, 220);

                if (m_GizmoMode == EGizmoMode::AxisX)
                {
                    draw_list->AddLine(center, axis_y_end, IM_COL32(100, 210, 130, 70), 1.5f);
                }
                if (m_GizmoMode == EGizmoMode::AxisY)
                {
                    draw_list->AddLine(center, axis_x_end, IM_COL32(235, 90, 90, 70), 1.5f);
                }

                draw_list->AddLine(center, axis_x_end, axis_x_color, 2.0f);
                draw_list->AddTriangleFilled(axis_x_end, ImVec2(axis_x_end.x - 9.0f, axis_x_end.y - 5.0f),
                    ImVec2(axis_x_end.x - 9.0f, axis_x_end.y + 5.0f), axis_x_color);
                draw_list->AddLine(center, axis_y_end, axis_y_color, 2.0f);
                draw_list->AddTriangleFilled(axis_y_end, ImVec2(axis_y_end.x - 5.0f, axis_y_end.y + 9.0f),
                    ImVec2(axis_y_end.x + 5.0f, axis_y_end.y + 9.0f), axis_y_color);

                draw_list->AddRectFilled(
                    ImVec2(center.x - 4.0f, center.y - 4.0f), ImVec2(center.x + 4.0f, center.y + 4.0f), handle_fill);
                draw_list->AddRect(
                    ImVec2(center.x - 4.0f, center.y - 4.0f), ImVec2(center.x + 4.0f, center.y + 4.0f), handle_edge);

                for (int index = 0; index < 4; ++index)
                {
                    const ImVec2 corner = object_corners[index];
                    draw_list->AddRectFilled(ImVec2(corner.x - 3.0f, corner.y - 3.0f),
                        ImVec2(corner.x + 3.0f, corner.y + 3.0f), handle_fill);
                    draw_list->AddRect(ImVec2(corner.x - 3.0f, corner.y - 3.0f),
                        ImVec2(corner.x + 3.0f, corner.y + 3.0f), handle_edge);
                }
            }
        }
        else if (editor->b_IsPlaying)
        {
            m_GizmoMode = EGizmoMode::None;
            m_bGizmoEntity = false;
            m_bGizmo3D = false;
        }

        // 2D gameplay entities get the same move gizmo as scene sprites. Size is
        // intentionally not exposed here: gameplay code owns what "size" means,
        // so the Inspector remains the place to edit it.
        if (!editor->b_IsPlaying && !editor->b_ActiveMapUses3DView())
        {
            const GameEditor::FEditorEntityRef* selected_entity = editor->GetSelectedEditorEntity();
            if ((selected_entity != nullptr) && (selected_entity->m_Position != nullptr))
            {
                if ((m_GizmoMode != EGizmoMode::None) &&
                    (!m_bGizmoEntity || (m_GizmoObjectId != selected_entity->m_Id)))
                {
                    m_GizmoMode = EGizmoMode::None;
                }

                const Rectangle rect = entity_rect(*selected_entity);
                const Vector2 entity_center_world = { rect.x + (rect.width * 0.5f), rect.y + (rect.height * 0.5f) };
                const FGizmo2D gizmo = MakeGizmo2D(world_to_screen(entity_center_world));
                constexpr float c_HandleRadius = 7.0f;

                const ImVec2 mouse = ImGui::GetIO().MousePos;
                const auto hover_circle = [&](ImVec2 point) -> bool
                {
                    return ((mouse.x - point.x) * (mouse.x - point.x)) + ((mouse.y - point.y) * (mouse.y - point.y)) <=
                        (c_HandleRadius * c_HandleRadius);
                };
                const auto hover_axis = [&](ImVec2 end) -> bool
                { return DistanceToSegment(mouse, gizmo.center, end) <= c_HandleRadius; };

                const bool b_hover_center = hover_circle(gizmo.center);
                const bool b_hover_x = hover_axis(gizmo.axis_x_end);
                const bool b_hover_y = hover_axis(gizmo.axis_y_end);

                if (b_hover_center || b_hover_x || b_hover_y)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                if ((m_GizmoMode == EGizmoMode::None) && b_image_hovered &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    if (b_hover_center)
                    {
                        m_GizmoMode = EGizmoMode::Move;
                    }
                    else if (b_hover_x)
                    {
                        m_GizmoMode = EGizmoMode::AxisX;
                    }
                    else if (b_hover_y)
                    {
                        m_GizmoMode = EGizmoMode::AxisY;
                    }

                    if (m_GizmoMode != EGizmoMode::None)
                    {
                        m_bGizmoEntity = true;
                        m_bGizmo3D = false;
                        m_GizmoObjectId = selected_entity->m_Id;
                        m_GizmoStartPosition = *selected_entity->m_Position;
                        m_GizmoStartWorldMouse = scene_to_world(screen_to_scene(mouse));

                        m_GizmoStarts.clear();
                        for (const uint64_t id : editor->GetSelectedEditorEntityIds())
                        {
                            for (const GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
                            {
                                if (entity.m_Id == id && entity.m_Position != nullptr)
                                {
                                    m_GizmoStarts.push_back({ 0, id, *entity.m_Position, {}, { 1.0f, 1.0f }, 0.0f });
                                    break;
                                }
                            }
                        }
                        b_gizmo_consumed = true;
                    }
                }

                if (m_bGizmoEntity && (m_GizmoMode != EGizmoMode::None) && (m_GizmoObjectId == selected_entity->m_Id))
                {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    {
                        const Vector2 world_mouse = scene_to_world(screen_to_scene(mouse));
                        Vector2 next_position = m_GizmoStartPosition;

                        switch (m_GizmoMode)
                        {
                            case EGizmoMode::Move:
                                next_position.x += world_mouse.x - m_GizmoStartWorldMouse.x;
                                next_position.y += world_mouse.y - m_GizmoStartWorldMouse.y;
                                break;
                            case EGizmoMode::AxisX:
                                next_position.x += world_mouse.x - m_GizmoStartWorldMouse.x;
                                break;
                            case EGizmoMode::AxisY:
                                next_position.y += world_mouse.y - m_GizmoStartWorldMouse.y;
                                break;
                            default:
                                break;
                        }

                        const Vector2 delta = { next_position.x - m_GizmoStartPosition.x,
                            next_position.y - m_GizmoStartPosition.y };
                        for (const FDragStart& start : m_GizmoStarts)
                        {
                            for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
                            {
                                if (entity.m_Id != start.m_EntityId || entity.m_Position == nullptr)
                                {
                                    continue;
                                }

                                Vector2 position = {
                                    start.m_Position.x + delta.x,
                                    start.m_Position.y + delta.y,
                                };
                                if (ImGui::GetIO().KeyCtrl && prefs.SceneSnapEnabled)
                                {
                                    const float snap_step = std::max(prefs.SceneSnapStep, 1.0f);
                                    position.x = std::round(position.x / snap_step) * snap_step;
                                    position.y = std::round(position.y / snap_step) * snap_step;
                                }
                                *entity.m_Position = position;
                                break;
                            }
                        }
                        b_gizmo_consumed = true;
                    }
                    else
                    {
                        std::vector<SceneCommands::FEntityTransformEntry> entries;
                        for (const FDragStart& start : m_GizmoStarts)
                        {
                            for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
                            {
                                if (entity.m_Id != start.m_EntityId || entity.m_Position == nullptr)
                                {
                                    continue;
                                }

                                if (start.m_Position.x == entity.m_Position->x &&
                                    start.m_Position.y == entity.m_Position->y)
                                {
                                    break;
                                }

                                SceneCommands::FEntityTransformEntry entry;
                                entry.m_Position = entity.m_Position;
                                entry.m_Before.m_Position = start.m_Position;
                                entry.m_After.m_Position = *entity.m_Position;
                                entries.push_back(entry);
                                break;
                            }
                        }
                        if (!entries.empty())
                        {
                            editor->GetCommandStack().Record(std::make_unique<SceneCommands::EntityTransformCommand>(
                                std::move(entries), "Move Entities"));
                        }
                        m_GizmoStarts.clear();
                        m_GizmoMode = EGizmoMode::None;
                        m_bGizmoEntity = false;
                    }
                }

                DrawGizmo2D(ImGui::GetWindowDrawList(), gizmo, m_GizmoMode == EGizmoMode::AxisX,
                    m_GizmoMode == EGizmoMode::AxisY);
            }
        }

        if (!editor->b_IsPlaying && editor->b_ActiveMapUses3DView())
        {
            const GameEditor::FEditorEntityRef* selected_entity = editor->GetSelectedEditorEntity();
            if ((selected_entity != nullptr) && (selected_entity->m_Position3D != nullptr))
            {
                if ((m_GizmoMode != EGizmoMode::None) && (!m_bGizmo3D || (m_GizmoObjectId != selected_entity->m_Id)))
                {
                    m_GizmoMode = EGizmoMode::None;
                }

                const Camera3D camera = editor->GetActiveViewCamera3D();
                const float scene_width = static_cast<float>(editor->m_SceneSettings.m_SceneWidth);
                const float scene_height = static_cast<float>(editor->m_SceneSettings.m_SceneHeight);
                const auto scene_pixel_to_screen = [image_min, scene_scale](ImVec2 point) -> ImVec2
                { return { image_min.x + (point.x * scene_scale), image_min.y + (point.y * scene_scale) }; };
                const auto project_world = [&](Vector3 world, ImVec2& out_screen) -> bool
                {
                    ImVec2 scene_pixel{};
                    if (!ProjectWorldToScene(world, camera, scene_width, scene_height, scene_pixel))
                    {
                        return false;
                    }
                    out_screen = scene_pixel_to_screen(scene_pixel);
                    return true;
                };

                Vector3 origin = *selected_entity->m_Position3D;
                origin.x += selected_entity->m_Origin3D.x;
                origin.y += selected_entity->m_Origin3D.y;
                origin.z += selected_entity->m_Origin3D.z;

                ImVec2 origin_screen{};
                if (!project_world(origin, origin_screen))
                {
                    m_GizmoMode = EGizmoMode::None;
                }
                else
                {
                    const float axis_length = 1.25f;
                    const ImU32 axis_x_color = IM_COL32(235, 90, 90, 230);
                    const ImU32 axis_y_color = IM_COL32(100, 210, 130, 230);
                    const ImU32 axis_z_color = IM_COL32(90, 140, 235, 230);
                    const ImU32 handle_fill = IM_COL32(245, 245, 245, 235);
                    const ImU32 handle_edge = IM_COL32(20, 20, 20, 220);

                    const auto draw_gizmo = [&]()
                    {
                        Vector3 world_origin = *selected_entity->m_Position3D;
                        world_origin.x += selected_entity->m_Origin3D.x;
                        world_origin.y += selected_entity->m_Origin3D.y;
                        world_origin.z += selected_entity->m_Origin3D.z;

                        ImVec2 center{};
                        ImVec2 x_end{};
                        ImVec2 y_end{};
                        ImVec2 z_end{};
                        const bool b_center = project_world(world_origin, center);
                        const bool b_x =
                            project_world({ world_origin.x + axis_length, world_origin.y, world_origin.z }, x_end);
                        const bool b_y =
                            project_world({ world_origin.x, world_origin.y + axis_length, world_origin.z }, y_end);
                        const bool b_z =
                            project_world({ world_origin.x, world_origin.y, world_origin.z + axis_length }, z_end);

                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        if (b_center && b_x)
                        {
                            draw_list->AddLine(center, x_end, axis_x_color, 2.0f);
                            draw_list->AddCircleFilled(x_end, 4.0f, axis_x_color);
                        }
                        if (b_center && b_y)
                        {
                            draw_list->AddLine(center, y_end, axis_y_color, 2.0f);
                            draw_list->AddCircleFilled(y_end, 4.0f, axis_y_color);
                        }
                        if (b_center && b_z)
                        {
                            draw_list->AddLine(center, z_end, axis_z_color, 2.0f);
                            draw_list->AddCircleFilled(z_end, 4.0f, axis_z_color);
                        }
                        if (b_center)
                        {
                            draw_list->AddRectFilled(ImVec2(center.x - 4.0f, center.y - 4.0f),
                                ImVec2(center.x + 4.0f, center.y + 4.0f), handle_fill);
                            draw_list->AddRect(ImVec2(center.x - 4.0f, center.y - 4.0f),
                                ImVec2(center.x + 4.0f, center.y + 4.0f), handle_edge);
                        }

                        if (selected_entity->m_Size3D != nullptr)
                        {
                            const Vector3 half = { selected_entity->m_Size3D->x * 0.5f,
                                selected_entity->m_Size3D->y * 0.5f, selected_entity->m_Size3D->z * 0.5f };
                            Vector3 corners[8];
                            int index = 0;
                            for (int z = -1; z <= 1; z += 2)
                            {
                                for (int y = -1; y <= 1; y += 2)
                                {
                                    for (int x = -1; x <= 1; x += 2)
                                    {
                                        corners[index++] = { world_origin.x + half.x * x, world_origin.y + half.y * y,
                                            world_origin.z + half.z * z };
                                    }
                                }
                            }

                            ImVec2 screen_corners[8];
                            bool b_valid[8]{};
                            for (int corner = 0; corner < 8; ++corner)
                            {
                                b_valid[corner] = project_world(corners[corner], screen_corners[corner]);
                            }

                            constexpr int edges[12][2] = { { 0, 1 }, { 0, 2 }, { 1, 3 }, { 2, 3 }, { 4, 5 }, { 4, 6 },
                                { 5, 7 }, { 6, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
                            for (const auto& edge : edges)
                            {
                                if (b_valid[edge[0]] && b_valid[edge[1]])
                                {
                                    draw_list->AddLine(screen_corners[edge[0]], screen_corners[edge[1]],
                                        IM_COL32(255, 255, 255, 90), 1.0f);
                                }
                            }
                        }
                    };

                    // Hover/hit test needs current handle positions.
                    ImVec2 x_end{};
                    ImVec2 y_end{};
                    ImVec2 z_end{};
                    const bool b_x = project_world({ origin.x + axis_length, origin.y, origin.z }, x_end);
                    const bool b_y = project_world({ origin.x, origin.y + axis_length, origin.z }, y_end);
                    const bool b_z = project_world({ origin.x, origin.y, origin.z + axis_length }, z_end);
                    const float radius = 8.0f;
                    const ImVec2 mouse = ImGui::GetIO().MousePos;

                    auto hover_point = [&](ImVec2 point) -> bool
                    {
                        return ((mouse.x - point.x) * (mouse.x - point.x)) +
                            ((mouse.y - point.y) * (mouse.y - point.y)) <=
                            radius * radius;
                    };

                    const bool b_hover_center = hover_point(origin_screen);
                    const bool b_hover_x = b_x && (DistanceToSegment(mouse, origin_screen, x_end) <= radius);
                    const bool b_hover_y = b_y && (DistanceToSegment(mouse, origin_screen, y_end) <= radius);
                    const bool b_hover_z = b_z && (DistanceToSegment(mouse, origin_screen, z_end) <= radius);

                    if (b_hover_center || b_hover_x || b_hover_y || b_hover_z)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    if ((m_GizmoMode == EGizmoMode::None) && b_image_hovered &&
                        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        if (b_hover_center)
                        {
                            m_GizmoMode = EGizmoMode::Move;
                        }
                        else if (b_hover_x)
                        {
                            m_GizmoMode = EGizmoMode::AxisX;
                        }
                        else if (b_hover_y)
                        {
                            m_GizmoMode = EGizmoMode::AxisY;
                        }
                        else if (b_hover_z)
                        {
                            m_GizmoMode = EGizmoMode::AxisZ;
                        }

                        if (m_GizmoMode != EGizmoMode::None)
                        {
                            m_bGizmo3D = true;
                            m_GizmoObjectId = selected_entity->m_Id;
                            m_GizmoStartPosition3D = *selected_entity->m_Position3D;
                            m_GizmoStartScreenMouse3D = { mouse.x, mouse.y };

                            m_GizmoStarts.clear();
                            for (const uint64_t id : editor->GetSelectedEditorEntityIds())
                            {
                                for (const GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
                                {
                                    if (entity.m_Id == id && entity.m_Position3D != nullptr)
                                    {
                                        FDragStart start;
                                        start.m_EntityId = id;
                                        start.m_Position3D = *entity.m_Position3D;
                                        m_GizmoStarts.push_back(start);
                                        break;
                                    }
                                }
                            }

                            if (m_GizmoMode == EGizmoMode::AxisY)
                            {
                                ImVec2 up_screen{};
                                if (project_world({ origin.x, origin.y + 1.0f, origin.z }, up_screen))
                                {
                                    const float pixels =
                                        std::sqrt(((up_screen.x - origin_screen.x) * (up_screen.x - origin_screen.x)) +
                                            ((up_screen.y - origin_screen.y) * (up_screen.y - origin_screen.y)));
                                    m_GizmoWorldPerPixel3D = 1.0f / std::max(pixels, 1.0f);
                                }
                            }
                            else
                            {
                                ImVec2 scene_mouse{ (mouse.x - image_min.x) / std::max(scene_scale, 0.0001f),
                                    (mouse.y - image_min.y) / std::max(scene_scale, 0.0001f) };
                                if (!UnprojectSceneToPlane(scene_mouse, camera, scene_width, scene_height, origin.y,
                                        m_GizmoStartWorldMouse3D))
                                {
                                    m_GizmoMode = EGizmoMode::None;
                                    m_bGizmo3D = false;
                                }
                            }
                            b_gizmo_consumed = true;
                        }
                    }

                    if ((m_GizmoMode != EGizmoMode::None) && m_bGizmo3D && (m_GizmoObjectId == selected_entity->m_Id))
                    {
                        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                        {
                            Vector3 next = m_GizmoStartPosition3D;
                            if (m_GizmoMode == EGizmoMode::AxisY)
                            {
                                next.y = m_GizmoStartPosition3D.y +
                                    ((m_GizmoStartScreenMouse3D.y - mouse.y) * m_GizmoWorldPerPixel3D);
                            }
                            else
                            {
                                ImVec2 scene_mouse{ (mouse.x - image_min.x) / std::max(scene_scale, 0.0001f),
                                    (mouse.y - image_min.y) / std::max(scene_scale, 0.0001f) };
                                Vector3 ground{};
                                if (UnprojectSceneToPlane(
                                        scene_mouse, camera, scene_width, scene_height, origin.y, ground))
                                {
                                    const Vector3 delta = Vector3Subtract(ground, m_GizmoStartWorldMouse3D);
                                    if (m_GizmoMode == EGizmoMode::Move)
                                    {
                                        next.x += delta.x;
                                        next.z += delta.z;
                                    }
                                    else if (m_GizmoMode == EGizmoMode::AxisX)
                                    {
                                        next.x += delta.x;
                                    }
                                    else if (m_GizmoMode == EGizmoMode::AxisZ)
                                    {
                                        next.z += delta.z;
                                    }
                                }
                            }

                            if (ImGui::GetIO().KeyCtrl)
                            {
                                constexpr float c_SNAP = 0.5f;
                                next.x = std::round(next.x / c_SNAP) * c_SNAP;
                                next.y = std::round(next.y / c_SNAP) * c_SNAP;
                                next.z = std::round(next.z / c_SNAP) * c_SNAP;
                            }

                            const Vector3 total_delta = { next.x - m_GizmoStartPosition3D.x,
                                next.y - m_GizmoStartPosition3D.y, next.z - m_GizmoStartPosition3D.z };
                            for (const FDragStart& start : m_GizmoStarts)
                            {
                                for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
                                {
                                    if (entity.m_Id != start.m_EntityId || entity.m_Position3D == nullptr)
                                    {
                                        continue;
                                    }

                                    Vector3 position = { start.m_Position3D.x + total_delta.x,
                                        start.m_Position3D.y + total_delta.y, start.m_Position3D.z + total_delta.z };
                                    if (ImGui::GetIO().KeyCtrl)
                                    {
                                        constexpr float c_SNAP = 0.5f;
                                        position.x = std::round(position.x / c_SNAP) * c_SNAP;
                                        position.y = std::round(position.y / c_SNAP) * c_SNAP;
                                        position.z = std::round(position.z / c_SNAP) * c_SNAP;
                                    }
                                    *entity.m_Position3D = position;
                                    break;
                                }
                            }
                            b_gizmo_consumed = true;
                        }
                        else
                        {
                            std::vector<SceneCommands::FEntityTransformEntry> entries;
                            for (const FDragStart& start : m_GizmoStarts)
                            {
                                for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
                                {
                                    if (entity.m_Id != start.m_EntityId || entity.m_Position3D == nullptr)
                                    {
                                        continue;
                                    }

                                    if (start.m_Position3D.x == entity.m_Position3D->x &&
                                        start.m_Position3D.y == entity.m_Position3D->y &&
                                        start.m_Position3D.z == entity.m_Position3D->z)
                                    {
                                        break;
                                    }

                                    SceneCommands::FEntityTransformEntry entry;
                                    entry.m_Position3D = entity.m_Position3D;
                                    entry.m_Before.m_Position3D = start.m_Position3D;
                                    entry.m_After.m_Position3D = *entity.m_Position3D;
                                    entries.push_back(entry);
                                    break;
                                }
                            }
                            if (!entries.empty())
                            {
                                editor->GetCommandStack().Record(
                                    std::make_unique<SceneCommands::EntityTransformCommand>(
                                        std::move(entries), "Move Entities"));
                            }
                            m_GizmoStarts.clear();
                            m_GizmoMode = EGizmoMode::None;
                        }
                    }

                    draw_gizmo();
                }
            }
            else if (m_bGizmo3D)
            {
                m_GizmoMode = EGizmoMode::None;
            }
        }

        if (!editor->b_IsPlaying)
        {
            // Grid/snap toggles live on the viewport only. Shift+G is spare:
            // the 3D camera reads W/A/S/D/Q/E only while the right button drags.
            if (!ImGui::GetIO().WantTextInput && !editor->GetTerminal().b_IsKeyboardCaptured() &&
                (b_image_hovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)))
            {
                if (ImGui::IsKeyChordPressed(ImGuiMod_Shift | ImGuiKey_G))
                {
                    prefs.SceneSnapEnabled = !prefs.SceneSnapEnabled;
                    EditorPreferences::GetInstance().m_bSaveToFile();
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_G, false))
                {
                    prefs.SceneGridEnabled = !prefs.SceneGridEnabled;
                    EditorPreferences::GetInstance().m_bSaveToFile();
                }
            }

            if (mutable_camera != nullptr)
            {
                ImGuiIO& io = ImGui::GetIO();

                if (b_image_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
                {
                    m_bPanning = true;
                }

                if (m_bPanning)
                {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
                    {
                        const Vector2 current_world = scene_to_world(screen_to_scene(io.MousePos));
                        const Vector2 previous_world = scene_to_world(screen_to_scene(
                            ImVec2{ io.MousePos.x - io.MouseDelta.x, io.MousePos.y - io.MouseDelta.y }));
                        Camera2DMath::Pan(*mutable_camera, previous_world, current_world);
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }
                    else
                    {
                        m_bPanning = false;
                    }
                }
                else if (b_image_hovered && io.MouseWheel != 0.0f)
                {
                    Camera2DMath::b_ZoomAt(*mutable_camera, screen_to_scene(io.MousePos), io.MouseWheel);
                }

                if (!io.WantTextInput &&
                    (b_image_hovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) &&
                    ImGui::IsKeyPressed(ImGuiKey_F))
                {
                    bool b_has_target = false;
                    Vector2 center{ 0.0f, 0.0f };

                    if (const GameEditor::FEditorEntityRef* entity = editor->GetSelectedEditorEntity())
                    {
                        if (entity->m_Position != nullptr)
                        {
                            const Rectangle rect = entity_rect(*entity);
                            center = { rect.x + (rect.width * 0.5f), rect.y + (rect.height * 0.5f) };
                            b_has_target = true;
                        }
                    }
                    else if (const t_SceneObject* object = editor->GetSelectedSceneObject())
                    {
                        const Rectangle rect = object_rect(*object);
                        center = { rect.x + (rect.width * 0.5f), rect.y + (rect.height * 0.5f) };
                        b_has_target = true;
                    }

                    if (b_has_target)
                    {
                        Camera2DMath::Frame(*mutable_camera, center);
                    }
                }
            }
            else
            {
                m_bPanning = false;
            }

            if (editor->b_ActiveMapUses3DView())
            {
                EditorCamera3D& editor_camera = editor->GetEditorCamera3D();
                ImGuiIO& io = ImGui::GetIO();

                if (b_image_hovered)
                {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
                    {
                        editor_camera.Look(io.MouseDelta.x, io.MouseDelta.y);
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                        b_camera_navigating = true;

                        const float move_right =
                            (ImGui::IsKeyDown(ImGuiKey_D) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_A) ? 1.0f : 0.0f);
                        const float move_forward =
                            (ImGui::IsKeyDown(ImGuiKey_W) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_S) ? 1.0f : 0.0f);
                        const float move_up =
                            (ImGui::IsKeyDown(ImGuiKey_E) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_Q) ? 1.0f : 0.0f);

                        if (!io.WantTextInput && ((move_right != 0.0f) || (move_forward != 0.0f) || (move_up != 0.0f)))
                        {
                            editor_camera.Move({ move_right, move_up, move_forward }, io.DeltaTime, io.KeyShift);
                        }
                    }
                    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                    {
                        editor_camera.Pan(io.MouseDelta.x, io.MouseDelta.y, image_size.y);
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        b_camera_navigating = true;
                    }
                    if (io.MouseWheel != 0.0f)
                    {
                        editor_camera.Dolly(io.MouseWheel);
                    }
                }

                if (!io.WantTextInput &&
                    (b_image_hovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) &&
                    ImGui::IsKeyPressed(ImGuiKey_F))
                {
                    if (const GameEditor::FEditorEntityRef* entity = editor->GetSelectedEditorEntity();
                        (entity != nullptr) && (entity->m_Position3D != nullptr))
                    {
                        Vector3 center = *entity->m_Position3D;
                        center.x += entity->m_Origin3D.x;
                        center.y += entity->m_Origin3D.y;
                        center.z += entity->m_Origin3D.z;

                        float radius = 1.0f;
                        if (entity->m_Size3D != nullptr)
                        {
                            center.x += entity->m_Size3D->x * 0.5f;
                            center.y += entity->m_Size3D->y * 0.5f;
                            center.z += entity->m_Size3D->z * 0.5f;
                            radius = std::max(0.1f, Vector3Length(*entity->m_Size3D) * 0.5f);
                        }

                        editor_camera.Focus(center, radius);
                    }
                }
            }

            // 3D click selection: ray against each entity's editor box.
            if (!editor->b_IsPlaying && editor->b_ActiveMapUses3DView() && !b_gizmo_consumed && !b_camera_navigating &&
                b_image_hovered && !m_bDragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const Camera3D camera_3d = editor->GetActiveViewCamera3D();
                const Vector2 scene_mouse = screen_to_scene(ImGui::GetMousePos());
                const Ray ray = GetScreenToWorldRayEx(scene_mouse, camera_3d, editor->m_SceneSettings.m_SceneWidth,
                    editor->m_SceneSettings.m_SceneHeight);

                float best_distance = std::numeric_limits<float>::max();
                uint64_t best_id = 0;
                for (const GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
                {
                    if ((entity.m_Position3D == nullptr) || ((entity.m_Visible != nullptr) && !*entity.m_Visible))
                    {
                        continue;
                    }

                    Vector3 half{ 0.5f, 0.5f, 0.5f };
                    if (entity.m_Size3D != nullptr)
                    {
                        half = { entity.m_Size3D->x * 0.5f, entity.m_Size3D->y * 0.5f, entity.m_Size3D->z * 0.5f };
                    }

                    const Vector3 center{ entity.m_Position3D->x + entity.m_Origin3D.x + half.x,
                        entity.m_Position3D->y + entity.m_Origin3D.y + half.y,
                        entity.m_Position3D->z + entity.m_Origin3D.z + half.z };
                    const BoundingBox box{ { center.x - half.x, center.y - half.y, center.z - half.z },
                        { center.x + half.x, center.y + half.y, center.z + half.z } };

                    const RayCollision hit = GetRayCollisionBox(ray, box);
                    if (hit.hit && hit.distance < best_distance)
                    {
                        best_distance = hit.distance;
                        best_id = entity.m_Id;
                    }
                }

                if (best_id != 0)
                {
                    const ImGuiIO& io = ImGui::GetIO();
                    if (io.KeyCtrl)
                    {
                        editor->ToggleEditorEntitySelection(best_id);
                    }
                    else if (io.KeyShift)
                    {
                        editor->AddEditorEntityToSelection(best_id);
                    }
                    else
                    {
                        editor->SelectEditorEntity(best_id);
                    }
                    b_gizmo_consumed = true;
                }
                else if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift)
                {
                    editor->ClearSelection();
                }
            }

            if (!b_gizmo_consumed && b_image_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_bDragging)
            {
                const ImGuiIO& io = ImGui::GetIO();
                const bool b_additive = io.KeyCtrl || io.KeyShift;
                const Vector2 scene_mouse = screen_to_scene(ImGui::GetMousePos());
                const Vector2 world_mouse = scene_to_world(scene_mouse);
                bool b_hit_entity = false;

                // Gameplay entities take priority over editor props.
                const auto& editor_entities = editor->GetEditorEntities();
                for (auto it = editor_entities.rbegin(); it != editor_entities.rend(); ++it)
                {
                    if (it->m_Position == nullptr)
                    {
                        continue;
                    }
                    if (it->m_Visible != nullptr && !*it->m_Visible)
                    {
                        continue;
                    }
                    if (!CheckCollisionPointRec(world_mouse, entity_rect(*it)))
                    {
                        continue;
                    }

                    b_hit_entity = true;
                    if (b_additive)
                    {
                        if (io.KeyCtrl)
                        {
                            editor->ToggleEditorEntitySelection(it->m_Id);
                        }
                        else
                        {
                            editor->AddEditorEntityToSelection(it->m_Id);
                        }
                        break;
                    }

                    if (editor->m_SelectionKind != GameEditor::EEditorSelectionKind::GameplayEntity ||
                        !editor->b_IsEditorEntitySelected(it->m_Id))
                    {
                        editor->SelectEditorEntity(it->m_Id);
                    }

                    // Drag the whole selection from its captured start values.
                    m_bDragging = true;
                    m_bDraggingEntity = true;
                    m_DragEntityId = it->m_Id;
                    m_DragStartWorldMouse = world_mouse;
                    m_DragStarts.clear();
                    for (const uint64_t id : editor->GetSelectedEditorEntityIds())
                    {
                        for (const GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
                        {
                            if (entity.m_Id == id && entity.m_Position != nullptr)
                            {
                                m_DragStarts.push_back({ 0, id, *entity.m_Position, {}, { 1.0f, 1.0f }, 0.0f });
                                break;
                            }
                        }
                    }
                    break;
                }

                if (!b_hit_entity)
                {
                    // Topmost first: higher layer wins, later object wins a tie.
                    std::vector<const t_SceneObject*> ordered;
                    ordered.reserve(scene.GetObjects().size());
                    for (const t_SceneObject& object : scene.GetObjects())
                    {
                        if (object.m_bVisible)
                        {
                            ordered.push_back(&object);
                        }
                    }
                    std::ranges::stable_sort(ordered, {}, [](const t_SceneObject* object) { return object->m_Layer; });

                    const t_SceneObject* hit = nullptr;
                    for (auto it = ordered.rbegin(); it != ordered.rend(); ++it)
                    {
                        if (CheckCollisionPointRec(world_mouse, object_rect(**it)))
                        {
                            hit = *it;
                            break;
                        }
                    }

                    if (hit != nullptr)
                    {
                        if (b_additive)
                        {
                            if (io.KeyCtrl)
                            {
                                editor->ToggleSceneObjectSelection(hit->m_Id);
                            }
                            else
                            {
                                editor->AddSceneObjectToSelection(hit->m_Id);
                            }
                        }
                        else
                        {
                            if (editor->m_SelectionKind != GameEditor::EEditorSelectionKind::SceneObject ||
                                !editor->b_IsSceneObjectSelected(hit->m_Id))
                            {
                                editor->SelectSceneObject(hit->m_Id);
                            }

                            m_bDragging = true;
                            m_bDraggingEntity = false;
                            m_DragObjectId = hit->m_Id;
                            m_DragStartWorldMouse = world_mouse;
                            m_DragStarts.clear();
                            for (const uint32_t id : editor->GetSelectedSceneObjectIds())
                            {
                                if (const t_SceneObject* object = scene.FindObject(id))
                                {
                                    m_DragStarts.push_back(
                                        { id, 0, object->m_Position, {}, object->m_Scale, object->m_Rotation });
                                }
                            }
                        }
                    }
                    else if (!b_additive)
                    {
                        editor->ClearSelection();
                    }
                    // Additive click on empty space keeps the existing set.
                }
            }

            if (m_bDragging)
            {
                const ImGuiIO& io = ImGui::GetIO();
                const Vector2 current_world = scene_to_world(screen_to_scene(io.MousePos));
                const Vector2 delta = { current_world.x - m_DragStartWorldMouse.x,
                    current_world.y - m_DragStartWorldMouse.y };

                if (m_bDraggingEntity)
                {
                    for (const FDragStart& start : m_DragStarts)
                    {
                        for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
                        {
                            if (entity.m_Id != start.m_EntityId || entity.m_Position == nullptr)
                            {
                                continue;
                            }

                            Vector2 position = { start.m_Position.x + delta.x, start.m_Position.y + delta.y };
                            if (io.KeyCtrl)
                            {
                                constexpr float c_SNAP = 8.0f;
                                position.x = std::round(position.x / c_SNAP) * c_SNAP;
                                position.y = std::round(position.y / c_SNAP) * c_SNAP;
                            }
                            *entity.m_Position = position;
                            break;
                        }
                    }
                }
                else
                {
                    bool b_moved = false;
                    for (const FDragStart& start : m_DragStarts)
                    {
                        t_SceneObject* object = scene.FindObject(start.m_SceneId);
                        if (object == nullptr)
                        {
                            continue;
                        }

                        Vector2 position = { start.m_Position.x + delta.x, start.m_Position.y + delta.y };
                        if (io.KeyCtrl)
                        {
                            constexpr float c_SNAP = 8.0f;
                            position.x = std::round(position.x / c_SNAP) * c_SNAP;
                            position.y = std::round(position.y / c_SNAP) * c_SNAP;
                        }
                        object->m_Position = position;
                        b_moved = true;
                    }
                    if (b_moved)
                    {
                        scene.MarkDirty();
                    }
                }

                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    if (m_bDraggingEntity)
                    {
                        std::vector<SceneCommands::FEntityTransformEntry> entries;
                        for (const FDragStart& start : m_DragStarts)
                        {
                            for (GameEditor::FEditorEntityRef& entity : editor->GetEditorEntities())
                            {
                                if (entity.m_Id != start.m_EntityId || entity.m_Position == nullptr)
                                {
                                    continue;
                                }
                                if (start.m_Position.x == entity.m_Position->x &&
                                    start.m_Position.y == entity.m_Position->y)
                                {
                                    break;
                                }

                                SceneCommands::FEntityTransformEntry entry;
                                entry.m_Position = entity.m_Position;
                                entry.m_Before.m_Position = start.m_Position;
                                entry.m_After.m_Position = *entity.m_Position;
                                entries.push_back(entry);
                                break;
                            }
                        }
                        if (!entries.empty())
                        {
                            editor->GetCommandStack().Record(std::make_unique<SceneCommands::EntityTransformCommand>(
                                std::move(entries), "Move Entities"));
                        }
                    }
                    else
                    {
                        std::vector<SceneCommands::FSceneObjectTransformEntry> entries;
                        for (const FDragStart& start : m_DragStarts)
                        {
                            t_SceneObject* object = scene.FindObject(start.m_SceneId);
                            if (object == nullptr)
                            {
                                continue;
                            }

                            const SceneCommands::FSceneObjectTransform after = SceneCommands::ReadTransform(*object);
                            if (start.m_Position.x == after.m_Position.x && start.m_Position.y == after.m_Position.y)
                            {
                                continue;
                            }

                            SceneCommands::FSceneObjectTransformEntry entry;
                            entry.m_Id = start.m_SceneId;
                            entry.m_Before = { start.m_Position, start.m_Rotation, start.m_Scale };
                            entry.m_After = after;
                            entries.push_back(entry);
                        }
                        if (!entries.empty())
                        {
                            editor->GetCommandStack().Record(
                                std::make_unique<SceneCommands::SceneObjectsTransformCommand>(
                                    scene, std::move(entries), "Move Objects"));
                        }
                    }

                    m_bDragging = false;
                    m_bDraggingEntity = false;
                    m_DragStarts.clear();
                }
            }
        }
        else
        {
            m_bDragging = false;
            m_bDraggingEntity = false;
            m_bPanning = false;
        }

        // Delete works from the viewport too: the hierarchy is not the only
        // place a multi-selection can be created.
        if (!editor->b_IsPlaying && !ImGui::GetIO().WantTextInput && !editor->GetTerminal().b_IsKeyboardCaptured() &&
            (b_image_hovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) &&
            ImGui::IsKeyPressed(ImGuiKey_Delete) &&
            editor->m_SelectionKind == GameEditor::EEditorSelectionKind::SceneObject &&
            !editor->GetSelectedSceneObjectIds().empty())
        {
            const std::vector<uint32_t> ids = editor->GetSelectedSceneObjectIds();
            const std::string prompt = ids.size() > 1
                ? "Delete the " + std::to_string(ids.size()) + " selected scene objects?"
                : std::string("Delete the selected scene object?");
            if (tinyfd_messageBox("Delete Object", prompt.c_str(), "yesno", "warning", 0) == 1)
            {
                editor->GetCommandStack().Push(std::make_unique<SceneCommands::SceneObjectRemoveCommand>(scene, ids));
                editor->ClearSelection();
            }
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RAYWAVES_ASSET"))
            {
                const std::string relative_path(
                    static_cast<const char*>(payload->Data), static_cast<size_t>(payload->DataSize) - 1);
                if (!relative_path.empty())
                {
                    const Vector2 position = scene_to_world(screen_to_scene(ImGui::GetMousePos()));
                    auto command = std::make_unique<SceneCommands::SceneObjectAddCommand>(
                        scene, "Sprite", position, relative_path);
                    auto* raw = command.get();
                    editor->GetCommandStack().Push(std::move(command));
                    editor->SelectSceneObject(raw->Id());
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (editor->b_IsPlaying && b_image_hovered)
        {
            ImGui::SetTooltip("Pause playback to edit.");
        }

        if (!editor->b_IsPlaying && mutable_camera != nullptr)
        {
            constexpr const char* c_HINT = "MMB Pan  |  Wheel Zoom  |  F Frame  |  G Grid  |  Shift+G Snap";
            const ImVec2 text_size = ImGui::CalcTextSize(c_HINT);
            const ImVec2 text_pos{ image_min.x + 8.0f, image_max.y - text_size.y - 8.0f };
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(ImVec2{ text_pos.x - 4.0f, text_pos.y - 2.0f },
                ImVec2{ text_pos.x + text_size.x + 4.0f, text_pos.y + text_size.y + 2.0f }, IM_COL32(0, 0, 0, 140),
                3.0f);
            draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), c_HINT);

            // Readout occupies the opposite corner so it never covers the hint.
            char position_text[96] = "x -  y -";
            if (b_image_hovered)
            {
                const Vector2 cursor_world =
                    GetScreenToWorld2D(screen_to_scene(ImGui::GetIO().MousePos), *mutable_camera);
                std::snprintf(position_text, sizeof(position_text), "x %.1f  y %.1f", cursor_world.x, cursor_world.y);
            }

            char state_text[96];
            std::snprintf(state_text, sizeof(state_text), "Zoom %d%%  |  Sel %d",
                static_cast<int>(std::lround(mutable_camera->zoom * 100.0f)),
                static_cast<int>(editor->GetSelectionCount()));

            const float line_height = ImGui::GetTextLineHeight();
            const float readout_width =
                std::max(ImGui::CalcTextSize(position_text).x, ImGui::CalcTextSize(state_text).x);

            // Small viewports cannot fit the hint and the readout side by side,
            // so the readout moves above the hint instead of overlapping it.
            const float readout_bottom_y =
                (image_size.x < (text_size.x + readout_width + 32.0f)) ? text_pos.y - 8.0f : image_max.y - 8.0f;
            const ImVec2 readout_max{ image_max.x - 8.0f, readout_bottom_y };
            const ImVec2 readout_min{ readout_max.x - readout_width - 8.0f,
                readout_max.y - (line_height * 2.0f) - 6.0f };
            draw_list->AddRectFilled(readout_min, readout_max, IM_COL32(0, 0, 0, 140), 3.0f);
            draw_list->AddText(ImVec2{ readout_min.x + 4.0f, readout_min.y + 3.0f },
                ImGui::GetColorU32(ImGuiCol_TextDisabled), position_text);
            draw_list->AddText(ImVec2{ readout_min.x + 4.0f, readout_min.y + 3.0f + line_height },
                ImGui::GetColorU32(ImGuiCol_Text), state_text);
        }
        else if (!editor->b_IsPlaying && mutable_camera == nullptr && b_image_hovered)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::End();
}
