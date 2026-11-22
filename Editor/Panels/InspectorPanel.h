#pragma once
#include "../SceneCommands.h"
#include "IEditorPanel.h"
#include <cstdint>
#include <vector>

class InspectorPanel : public IEditorPanel
{
public:
    // Edit trackers live on the panel because ImGui only reports activation and
    // deactivation while the widget is drawn; the value captured at activation
    // has to survive into the commit frame.
    struct FSceneFieldEdit
    {
        bool m_bEditing = false;
        uint32_t m_ObjectId = 0;
        SceneCommands::ESceneObjectField m_Field = SceneCommands::ESceneObjectField::Name;
        SceneCommands::FSceneObjectFieldValue m_Before;
    };

    struct FSceneBatchEdit
    {
        bool m_bEditing = false;
        int m_Field = -1;
        std::vector<uint32_t> m_Ids;
        std::vector<SceneCommands::FSceneObjectTransform> m_BeforeTransforms;
    };

    struct FEntityFieldEdit
    {
        bool m_bEditing = false;
        uint64_t m_EntityId = 0;
        int m_Field = -1;
        SceneCommands::FEntityTransform m_Before;
    };

    struct FEntityPropertyEdit
    {
        bool m_bEditing = false;
        uint64_t m_EntityId = 0;
        uint32_t m_PropertyIndex = 0;
        SceneCommands::FEntityPropertyValue m_Before;
    };

    struct FEntityBatchEdit
    {
        bool m_bEditing = false;
        int m_Field = -1;
        std::vector<uint64_t> m_Ids;
        std::vector<SceneCommands::FEntityTransform> m_BeforeTransforms;
    };

    InspectorPanel() = default;
    ~InspectorPanel() override = default;

    void Draw(GameEditor* editor) override;

    // Public because the free draw helpers in the .cpp drive these trackers.
    FSceneFieldEdit m_SceneFieldEdit;
    FSceneBatchEdit m_SceneBatchEdit;
    FEntityFieldEdit m_EntityFieldEdit;
    FEntityPropertyEdit m_EntityPropertyEdit;
    FEntityBatchEdit m_EntityBatchEdit;
};
