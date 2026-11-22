#include "LogPanel.h"
#include "../EditorWidgets.h"
#include "../GameEditor.h"
#include "../LogBuffer.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <imgui.h>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::string ToLower(std::string_view text)
{
    std::string lowered(text);
    std::ranges::transform(
        lowered, lowered.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return lowered;
}

const char* LevelName(Log::ELevel level)
{
    switch (level)
    {
        case Log::ELevel::Debug:
            return "Debug";
        case Log::ELevel::Info:
            return "Info";
        case Log::ELevel::Warn:
            return "Warn";
        case Log::ELevel::Error:
            return "Error";
    }
    return "Info";
}

ImVec4 LevelColor(Log::ELevel level)
{
    switch (level)
    {
        case Log::ELevel::Debug:
            return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        case Log::ELevel::Info:
            return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        case Log::ELevel::Warn:
            return ImVec4(1.0f, 0.85f, 0.2f, 1.0f);
        case Log::ELevel::Error:
            return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    }
    return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

std::string FormatTime(std::chrono::system_clock::time_point time)
{
    const std::time_t seconds = std::chrono::system_clock::to_time_t(time);
    // localtime is not thread-safe, but this only runs on the UI thread.
    const std::tm* local = std::localtime(&seconds);
    char time_text[16] = "--:--:--";
    if (local != nullptr)
    {
        std::strftime(time_text, sizeof(time_text), "%H:%M:%S", local);
    }

    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%s.%03d", time_text, static_cast<int>(millis.count()));
    return buffer;
}

size_t LevelIndex(Log::ELevel level)
{
    return static_cast<size_t>(level);
}
} // namespace

void LogPanel::Draw(GameEditor* editor)
{
    if (!editor->m_bShowLog)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(900.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(ICON_FA_LIST " Log###LogPanel", &editor->m_bShowLog))
    {
        ImGui::End();
        return;
    }

    bool b_copy_requested = false;
    const float icon_size = 24.0f;
    if (EditorWidgets::IconButton("log_clear", ICON_FA_BROOM, "Clear log", ImVec2(icon_size, icon_size)))
    {
        editor->GetLogBuffer().Clear();
    }
    ImGui::SameLine();
    if (EditorWidgets::IconButton(
            "log_copy_all", ICON_FA_COPY, "Copy all visible entries", ImVec2(icon_size, icon_size)))
    {
        b_copy_requested = true;
    }
    ImGui::SameLine();
    EditorWidgets::ClearableFilter("log_filter", m_Filter, "Filter (case-insensitive)...");

    ImGui::Checkbox("Debug", &m_LevelEnabled[LevelIndex(Log::ELevel::Debug)]);
    ImGui::SameLine();
    ImGui::Checkbox("Info", &m_LevelEnabled[LevelIndex(Log::ELevel::Info)]);
    ImGui::SameLine();
    ImGui::Checkbox("Warn", &m_LevelEnabled[LevelIndex(Log::ELevel::Warn)]);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &m_LevelEnabled[LevelIndex(Log::ELevel::Error)]);
    ImGui::SameLine();
    ImGui::TextDisabled("|");

    const std::vector<FLogEntry> entries = editor->GetLogBuffer().Snapshot();
    const std::string filter_lower = ToLower(m_Filter);
    std::vector<const FLogEntry*> visible;
    visible.reserve(entries.size());
    for (const FLogEntry& entry : entries)
    {
        if (!m_LevelEnabled[LevelIndex(entry.m_Level)])
        {
            continue;
        }
        if (!filter_lower.empty() && !ToLower(entry.m_Category).contains(filter_lower) &&
            !ToLower(entry.m_Message).contains(filter_lower))
        {
            continue;
        }
        visible.push_back(&entry);
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %zu entries", visible.size(), entries.size());

    if (b_copy_requested)
    {
        std::string text;
        for (const FLogEntry* entry : visible)
        {
            text += FormatTime(entry->m_Time);
            text += " [";
            text += LevelName(entry->m_Level);
            text += "] ";
            text += entry->m_Category;
            text += ": ";
            text += entry->m_Message;
            text += '\n';
        }
        ImGui::SetClipboardText(text.c_str());
    }

    const ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("LogTable", 4, flags))
    {
        ImGui::End();
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 160.0f);
    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Only follow the tail when the user has not scrolled away from it.
    const bool b_was_at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

    if (visible.empty())
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("No log entries match the current filter.");
    }

    for (const FLogEntry* entry : visible)
    {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(FormatTime(entry->m_Time).c_str());

        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(entry->m_Level));
        ImGui::TextUnformatted(LevelName(entry->m_Level));
        ImGui::PopStyleColor();

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(entry->m_Category.c_str());

        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", entry->m_Message.c_str());
    }

    if (b_was_at_bottom)
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndTable();

    ImGui::End();
}
