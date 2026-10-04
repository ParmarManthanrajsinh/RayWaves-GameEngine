#pragma once
#include "IEditorPanel.h"
#include "../../Engine/Profiler.h"
#include <vector>

class PerformanceOverlay : public IEditorPanel
{
public:
    PerformanceOverlay() = default;
    ~PerformanceOverlay() override = default;

    void Draw(GameEditor* editor) override;

private:
    // GetAverages() rebuilds a vector + ordered set every call; refresh the
    // cached snapshot at ~4 Hz instead of every frame while visible.
    static constexpr int c_SnapshotRefreshFrames = 15;
    int m_SnapshotCountdown = 0;
    std::vector<ProfilerSnapshot> m_Snapshots;
};
