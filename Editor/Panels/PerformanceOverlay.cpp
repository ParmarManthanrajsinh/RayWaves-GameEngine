#include "PerformanceOverlay.h"
#include "../GameEditor.h"
#include <imgui.h>
#include <rlImGui.h>

void PerformanceOverlay::Draw(GameEditor* editor)
{
	if (!editor->m_bShowPerformanceStats)
	{
		return;
	}

	ImGui::SetNextWindowBgAlpha(0.7f);

	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoNav;

	if (ImGui::Begin("Performance Overlay", &editor->m_bShowPerformanceStats, window_flags))
	{
		float avg_frame_time = 0.0f;
		float max_frame_time = 0.0f;

		for (float t : editor->m_FrameTimes)
		{
			avg_frame_time += t;
			if (t > max_frame_time) max_frame_time = t;
		}
		avg_frame_time /= editor->m_FrameTimes.size();

		float fps = 1000.0f / (avg_frame_time > 0.001f ? avg_frame_time : 16.66f);

		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[ImGui::GetIO().Fonts->Fonts.size() > 1 ? 1 : 0]); 
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%.0f FPS", fps);
		ImGui::PopFont();

		ImGui::Separator();
		ImGui::Text("Avg: %.2f ms", avg_frame_time);
		ImGui::Text("Max: %.2f ms", max_frame_time);

		ImGui::Spacing();

		ImGui::PlotLines
		(
			"##FrameTimes",
			editor->m_FrameTimes.data(),
			static_cast<int>(editor->m_FrameTimes.size()),
			static_cast<int>(editor->m_FrameOffset),
			"Frame Time (ms)",
			0.0f,
			33.0f,
			ImVec2(200, 60)
		);
	}
	ImGui::End();
}
