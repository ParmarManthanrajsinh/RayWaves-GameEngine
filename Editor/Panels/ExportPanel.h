#pragma once
#include "IEditorPanel.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

class ExportPanel : public IEditorPanel
{
public:
    ExportPanel() = default;
    ~ExportPanel() override = default;

    void Draw(GameEditor* editor) override;

private:
    // Root of the project whose export settings are currently loaded into the
    // editor state. A change means the panel must reload from the manifest.
    std::string m_LoadedProjectRoot;

    // Session-only export option. The project manifest has no clean field, so
    // this is deliberately not persisted.
    bool m_bCleanOutput = false;

    // Cancel flag handed to the running export. Shared with the editor shutdown
    // flag so closing the window or cancelling from the panel stops the worker.
    std::shared_ptr<std::atomic<bool>> m_ExportCancel;
    std::atomic<float> m_ExportProgress{ 0.0f };

    // Stage text and the final output path cross the worker/UI boundary, so
    // they are guarded instead of being read while the worker writes them.
    std::mutex m_ExportStatusMutex;
    std::string m_ExportStage;
    std::string m_OutputPath;
};
