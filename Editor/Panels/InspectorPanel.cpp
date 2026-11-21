#include "InspectorPanel.h"
#include "../../Engine/MapManager.h"
#include "../../Engine/Profiler.h"
#include "../../Engine/ProjectManager.h"
#include "../EditorAssets.h"
#include "../GameEditor.h"
#include <algorithm>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <string>
#include <tinyfiledialogs.h>

namespace
{
constexpr float c_LabelWidth = 92.0f;
constexpr ImU32 c_AxisXColor = IM_COL32(235, 90, 90, 255);
constexpr ImU32 c_AxisYColor = IM_COL32(100, 210, 130, 255);
constexpr ImU32 c_AxisZColor = IM_COL32(90, 140, 235, 255);

void SetTexture(GameEditor* editor, t_SceneObject& object, const std::string& path)
{
    object.m_TexturePath = path;
    editor->GetScene().ReloadTexture(object.m_Id);
}

// Left-aligned label column, so the widgets that follow never cover their own
// label (the old full-width InputFloat2 labels ended up clipped off-screen).
void PropertyLabel(const char* label)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(c_LabelWidth);
}

bool AxisField(
    const char* id, const char* axis, float* value, ImU32 axis_color, float width, const char* format = "%.1f")
{
    ImGui::PushStyleColor(ImGuiCol_Text, axis_color);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(axis);
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 3.0f);
    ImGui::SetNextItemWidth(width);
    return ImGui::DragFloat(id, value, 1.0f, 0.0f, 0.0f, format);
}

bool Vector2Row(const char* label, const char* id_prefix, float* values, const char* format = "%.1f")
{
    PropertyLabel(label);
    const float field_width = std::max(54.0f, (ImGui::GetContentRegionAvail().x - 36.0f) * 0.5f);
    const std::string x_id = std::string("##") + id_prefix + "_x";
    const std::string y_id = std::string("##") + id_prefix + "_y";

    bool b_changed = AxisField(x_id.c_str(), "X", &values[0], c_AxisXColor, field_width, format);
    ImGui::SameLine();
    b_changed |= AxisField(y_id.c_str(), "Y", &values[1], c_AxisYColor, field_width, format);
    return b_changed;
}

bool Vector3Row(const char* label, const char* id_prefix, float* values, const char* format = "%.1f")
{
    PropertyLabel(label);
    const float field_width = std::max(46.0f, (ImGui::GetContentRegionAvail().x - 56.0f) / 3.0f);
    const std::string x_id = std::string("##") + id_prefix + "_x";
    const std::string y_id = std::string("##") + id_prefix + "_y";
    const std::string z_id = std::string("##") + id_prefix + "_z";

    bool b_changed = AxisField(x_id.c_str(), "X", &values[0], c_AxisXColor, field_width, format);
    ImGui::SameLine();
    b_changed |= AxisField(y_id.c_str(), "Y", &values[1], c_AxisYColor, field_width, format);
    ImGui::SameLine();
    b_changed |= AxisField(z_id.c_str(), "Z", &values[2], c_AxisZColor, field_width, format);
    return b_changed;
}

bool FloatRow(const char* label, const char* id, float* value, const char* format = "%.1f")
{
    PropertyLabel(label);
    ImGui::SetNextItemWidth(-1.0f);
    return ImGui::DragFloat(id, value, 1.0f, 0.0f, 0.0f, format);
}

bool BoolRow(const char* label, const char* id, bool* value)
{
    PropertyLabel(label);
    return ImGui::Checkbox(id, value);
}

void DrawEntityHeader(const GameEditor::FEditorEntityRef& entity)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
    ImGui::Text("%s  %s", ICON_FA_GAMEPAD, entity.m_Name.c_str());
    ImGui::PopStyleColor();

    ImGui::TextDisabled("%s", entity.m_Type.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("- gameplay object - id %llu", static_cast<unsigned long long>(entity.m_Id));
}

void DrawEntityProperties(const GameEditor::FEditorEntityRef& entity)
{
    if ((entity.m_Properties == nullptr) || (entity.m_PropertyCount == 0))
    {
        return;
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Properties");
    ImGui::PushID(static_cast<int>(entity.m_Id));
    for (uint32_t index = 0; index < entity.m_PropertyCount; ++index)
    {
        t_EntityProperty& property = entity.m_Properties[index];
        if ((property.m_Name == nullptr) || (property.m_Data == nullptr))
        {
            continue;
        }

        ImGui::PushID(static_cast<int>(index));
        switch (property.m_Type)
        {
            case EEntityPropertyType::Float:
            {
                PropertyLabel(property.m_Name);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragFloat("##value", static_cast<float*>(property.m_Data), 0.1f, property.m_Min, property.m_Max);
                break;
            }
            case EEntityPropertyType::Int:
            {
                PropertyLabel(property.m_Name);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragInt("##value", static_cast<int*>(property.m_Data), 1.0f, static_cast<int>(property.m_Min),
                    static_cast<int>(property.m_Max));
                break;
            }
            case EEntityPropertyType::Bool:
            {
                ImGui::Checkbox(property.m_Name, static_cast<bool*>(property.m_Data));
                break;
            }
            case EEntityPropertyType::Vector2:
            {
                Vector2Row(property.m_Name, property.m_Name, &static_cast<Vector2*>(property.m_Data)->x, "%.2f");
                break;
            }
        }
        ImGui::PopID();
    }
    ImGui::PopID();
}

void DrawEntityInspector(GameEditor* editor, GameEditor::FEditorEntityRef& entity)
{
    DrawEntityHeader(entity);
    ImGui::Spacing();

    if (editor->b_IsPlaying)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.35f, 1.0f));
        ImGui::TextUnformatted(ICON_FA_PAUSE "  Playback running - stop to edit");
        ImGui::PopStyleColor();
    }

    ImGui::SeparatorText("Transform");

    if (editor->b_IsPlaying)
    {
        ImGui::BeginDisabled();
    }

    if (entity.m_Position != nullptr)
    {
        Vector2Row("Position", "entity_position", &entity.m_Position->x);
    }
    else if (entity.m_Position3D != nullptr)
    {
        Vector3Row("Position", "entity_position3d", &entity.m_Position3D->x);
    }

    if (entity.m_Size != nullptr)
    {
        Vector2Row("Size", "entity_size", &entity.m_Size->x);
    }
    else if (entity.m_Size3D != nullptr)
    {
        Vector3Row("Size", "entity_size3d", &entity.m_Size3D->x);
    }
    else
    {
        PropertyLabel("Size");
        ImGui::TextDisabled("not exposed");
    }

    if (entity.m_Rotation != nullptr)
    {
        FloatRow("Rotation", "##entity_rotation", entity.m_Rotation, "%.1f deg");
    }

    if (entity.m_Visible != nullptr)
    {
        BoolRow("Visible", "##entity_visible", entity.m_Visible);
    }

    if (editor->b_IsPlaying)
    {
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Gameplay");

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("These values point straight into your GameLogic objects.");
    ImGui::TextDisabled("The editor only writes them; gameplay code decides what they mean.");
    ImGui::PopTextWrapPos();

    DrawEntityProperties(entity);

    std::string exposed;
    const auto add_exposed = [&exposed](const char* name)
    {
        if (!exposed.empty())
        {
            exposed += ", ";
        }
        exposed += name;
    };
    if ((entity.m_Position != nullptr) || (entity.m_Position3D != nullptr))
    {
        add_exposed("position");
    }
    if ((entity.m_Size != nullptr) || (entity.m_Size3D != nullptr))
    {
        add_exposed("size");
    }
    if (entity.m_Rotation != nullptr)
    {
        add_exposed("rotation");
    }
    if (entity.m_Visible != nullptr)
    {
        add_exposed("visibility");
    }
    for (uint32_t index = 0; index < entity.m_PropertyCount; ++index)
    {
        if (entity.m_Properties[index].m_Name != nullptr)
        {
            add_exposed(entity.m_Properties[index].m_Name);
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Exposed: %s", exposed.empty() ? "nothing" : exposed.c_str());
}

void DrawTextureSection(GameEditor* editor, t_SceneObject& object, bool& b_changed)
{
    ImGui::SeparatorText("Appearance");

    const bool b_has_texture = IsTextureValid(object.m_Texture);
    if (b_has_texture)
    {
        const float preview_height = 64.0f;
        const float aspect = (object.m_Texture.height > 0)
            ? static_cast<float>(object.m_Texture.width) / static_cast<float>(object.m_Texture.height)
            : 1.0f;
        const ImVec2 preview_size{ preview_height * aspect, preview_height };
        ImGui::Image(static_cast<ImTextureID>(object.m_Texture.id), preview_size, ImVec2(0, 0), ImVec2(1, 1));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("%d x %d px", object.m_Texture.width, object.m_Texture.height);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", object.m_TexturePath.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
    }
    else if (!object.m_TexturePath.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION " Texture not found");
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", object.m_TexturePath.c_str());
        ImGui::PopTextWrapPos();
    }
    else
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("No texture - drawn as a placeholder.");
        ImGui::PopTextWrapPos();
    }

    ImGui::Spacing();

    PropertyLabel("Texture");
    ImGui::SetNextItemWidth(-1.0f);
    std::string texture_path = object.m_TexturePath;
    if (ImGui::InputTextWithHint("##texture_path", "Assets/crate.png", &texture_path))
    {
        object.m_TexturePath = texture_path;
        b_changed = true;
    }

    // Loading a texture per keystroke would thrash the GPU and spam missing
    // paths, so reload only after the field loses focus.
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        SetTexture(editor, object, object.m_TexturePath);
    }

    const float button_width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse", ImVec2(button_width, 0.0f)))
    {
        const std::string asset_root = ProjectManager::GetCurrentCopy().m_AssetPath;
        const char* filters[] = { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga", "*.gif", "*.qoi", "*.psd" };
        const char* selected =
            tinyfd_openFileDialog("Select Texture", asset_root.c_str(), 8, filters, "Image files", 0);
        if (selected != nullptr)
        {
            std::string relative;
            if (EditorAssets::b_ImportFile(selected, relative))
            {
                SetTexture(editor, object, relative);
                b_changed = true;
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_XMARK " Clear", ImVec2(button_width, 0.0f)))
    {
        SetTexture(editor, object, "");
        b_changed = true;
    }
}

void DrawSceneObjectInspector(GameEditor* editor, t_SceneObject& object)
{
    Scene& scene = editor->GetScene();
    bool b_changed = false;

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
    ImGui::Text("%s  %s", ICON_FA_IMAGE, object.m_Name.empty() ? "Object" : object.m_Name.c_str());
    ImGui::PopStyleColor();
    ImGui::TextDisabled("Scene sprite - id %u - layer %d", object.m_Id, object.m_Layer);
    ImGui::Spacing();

    PropertyLabel("Name");
    ImGui::SetNextItemWidth(-1.0f);
    b_changed |= ImGui::InputTextWithHint("##object_name", "Object name", &object.m_Name);

    PropertyLabel("Type / Tag");
    ImGui::SetNextItemWidth(-1.0f);
    b_changed |= ImGui::InputTextWithHint("##object_type", "spawn, enemy, decor", &object.m_Type);

    ImGui::Spacing();

    DrawTextureSection(editor, object, b_changed);

    ImGui::SeparatorText("Transform");
    b_changed |= Vector2Row("Position", "object_position", &object.m_Position.x);
    b_changed |= FloatRow("Rotation", "##object_rotation", &object.m_Rotation, "%.1f deg");
    b_changed |= Vector2Row("Scale", "object_scale", &object.m_Scale.x, "%.2f");

    if (ImGui::CollapsingHeader("Sprite Region"))
    {
        float source[4] = { object.m_Source.x, object.m_Source.y, object.m_Source.width, object.m_Source.height };
        PropertyLabel("Source");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat4("##object_source", source, 1.0f, 0.0f, 0.0f, "%.0f"))
        {
            object.m_Source = { source[0], source[1], source[2], source[3] };
            b_changed = true;
        }
    }

    ImGui::SeparatorText("Rendering");

    PropertyLabel("Tint");
    float tint[4] = { static_cast<float>(object.m_Tint.r) / 255.0f, static_cast<float>(object.m_Tint.g) / 255.0f,
        static_cast<float>(object.m_Tint.b) / 255.0f, static_cast<float>(object.m_Tint.a) / 255.0f };
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::ColorEdit4("##object_tint", tint, ImGuiColorEditFlags_AlphaBar))
    {
        object.m_Tint =
            Color{ static_cast<unsigned char>(tint[0] * 255.0f), static_cast<unsigned char>(tint[1] * 255.0f),
                static_cast<unsigned char>(tint[2] * 255.0f), static_cast<unsigned char>(tint[3] * 255.0f) };
        b_changed = true;
    }

    PropertyLabel("Layer");
    ImGui::SetNextItemWidth(-1.0f);
    b_changed |= ImGui::DragInt("##object_layer", &object.m_Layer, 0.2f);
    b_changed |= BoolRow("Visible", "##object_visible", &object.m_bVisible);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.16f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.22f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.78f, 0.26f, 0.28f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH_CAN " Delete Object", ImVec2(-1.0f, 0.0f)))
    {
        if (tinyfd_messageBox("Delete Object", "Delete the selected object?", "yesno", "warning", 0) == 1)
        {
            scene.b_RemoveObject(object.m_Id);
            editor->ClearSelection();
            ImGui::PopStyleColor(3);
            return;
        }
    }
    ImGui::PopStyleColor(3);

    if (b_changed)
    {
        scene.MarkDirty();
    }
}
} // namespace

void InspectorPanel::Draw(GameEditor* editor)
{
    SCOPED_TIMER("panel_inspector");
    if (!ImGui::Begin(ICON_FA_SLIDERS " Inspector", &editor->m_bShowInspector))
    {
        ImGui::End();
        return;
    }

    if (!ProjectManager::b_HasOpenProject())
    {
        ImGui::TextDisabled("Open a project to inspect its scene.");
        ImGui::End();
        return;
    }

    if (GameEditor::FEditorEntityRef* entity = editor->GetSelectedEditorEntity())
    {
        DrawEntityInspector(editor, *entity);
        ImGui::End();
        return;
    }

    if (t_SceneObject* object = editor->GetSelectedSceneObject())
    {
        DrawSceneObjectInspector(editor, *object);
        ImGui::End();
        return;
    }

    MapManager* manager = editor->GetMapManager();
    if (manager != nullptr)
    {
        ImGui::Text("Current level: %s", std::string(manager->GetCurrentMapId()).c_str());
    }
    else
    {
        ImGui::Text("Current level: (none)");
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled(ICON_FA_HAND_POINTER "  Nothing selected");
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Pick a gameplay object in the Scene panel, or a sprite in the Viewport.");
    ImGui::PopTextWrapPos();

    ImGui::End();
}
