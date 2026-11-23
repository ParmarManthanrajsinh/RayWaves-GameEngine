#pragma once
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <raylib.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace rw
{
// Deterministic input replay for agent runs.
//
// Default behavior is plain raylib. When RAYWAVES_AGENT_INPUT points at a
// script, keys and mouse are driven from that file instead. The script lives
// inside the GameLogic module, so the host only has to set the environment
// variable before loading the DLL.
//
// Format (frame,KEY_NAME,down|up):
//
//     0,KEY_RIGHT,down
//     30,KEY_RIGHT,up
//
// Mouse position: 0,mouse,640,360
namespace input_detail
{
struct FInputEvent
{
    int m_Frame = 0;
    int m_Key = 0;
    bool m_bDown = false;
};

struct FMouseEvent
{
    int m_Frame = 0;
    Vector2 m_Position{ 0.0f, 0.0f };
};

struct FState
{
    bool m_bLoaded = false;
    bool m_bEnabled = false;
    int m_Frame = -1;
    std::vector<FInputEvent> m_Events;
    std::vector<FMouseEvent> m_MouseEvents;
    size_t m_EventCursor = 0;
    size_t m_MouseCursor = 0;
    std::unordered_map<int, bool> m_Down;
    std::unordered_set<int> m_Pressed;
    std::unordered_set<int> m_Released;
    Vector2 m_MousePosition{ 0.0f, 0.0f };
    bool m_bHasMousePosition = false;
};

inline std::string_view TrimInput(std::string_view value)
{
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
    {
        return {};
    }
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, (end - start) + 1);
}

inline int ParseKeyName(std::string_view raw)
{
    std::string name(raw);
    for (char& character : name)
    {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }

    if (name.size() > 1 && name.front() >= '0' && name.front() <= '9' &&
        std::all_of(name.begin(), name.end(), [](char c) { return c >= '0' && c <= '9'; }))
    {
        return std::atoi(name.c_str());
    }

    if (name.rfind("KEY_", 0) == 0)
    {
        name.erase(0, 4);
    }

    static const std::pair<const char*, int> c_NAMES[] = {
        { "A", KEY_A },
        { "B", KEY_B },
        { "C", KEY_C },
        { "D", KEY_D },
        { "E", KEY_E },
        { "F", KEY_F },
        { "G", KEY_G },
        { "H", KEY_H },
        { "I", KEY_I },
        { "J", KEY_J },
        { "K", KEY_K },
        { "L", KEY_L },
        { "M", KEY_M },
        { "N", KEY_N },
        { "O", KEY_O },
        { "P", KEY_P },
        { "Q", KEY_Q },
        { "R", KEY_R },
        { "S", KEY_S },
        { "T", KEY_T },
        { "U", KEY_U },
        { "V", KEY_V },
        { "W", KEY_W },
        { "X", KEY_X },
        { "Y", KEY_Y },
        { "Z", KEY_Z },
        { "0", KEY_ZERO },
        { "1", KEY_ONE },
        { "2", KEY_TWO },
        { "3", KEY_THREE },
        { "4", KEY_FOUR },
        { "5", KEY_FIVE },
        { "6", KEY_SIX },
        { "7", KEY_SEVEN },
        { "8", KEY_EIGHT },
        { "9", KEY_NINE },
        { "LEFT", KEY_LEFT },
        { "RIGHT", KEY_RIGHT },
        { "UP", KEY_UP },
        { "DOWN", KEY_DOWN },
        { "SPACE", KEY_SPACE },
        { "ENTER", KEY_ENTER },
        { "ESCAPE", KEY_ESCAPE },
        { "TAB", KEY_TAB },
        { "BACKSPACE", KEY_BACKSPACE },
        { "INSERT", KEY_INSERT },
        { "DELETE", KEY_DELETE },
        { "HOME", KEY_HOME },
        { "END", KEY_END },
        { "PAGE_UP", KEY_PAGE_UP },
        { "PAGE_DOWN", KEY_PAGE_DOWN },
        { "LEFT_SHIFT", KEY_LEFT_SHIFT },
        { "RIGHT_SHIFT", KEY_RIGHT_SHIFT },
        { "LEFT_CONTROL", KEY_LEFT_CONTROL },
        { "RIGHT_CONTROL", KEY_RIGHT_CONTROL },
        { "LEFT_ALT", KEY_LEFT_ALT },
        { "RIGHT_ALT", KEY_RIGHT_ALT },
        { "F1", KEY_F1 },
        { "F2", KEY_F2 },
        { "F3", KEY_F3 },
        { "F4", KEY_F4 },
        { "F5", KEY_F5 },
        { "F6", KEY_F6 },
        { "F7", KEY_F7 },
        { "F8", KEY_F8 },
        { "F9", KEY_F9 },
        { "F10", KEY_F10 },
        { "F11", KEY_F11 },
        { "F12", KEY_F12 },
        { "MOUSE_LEFT", MOUSE_BUTTON_LEFT },
        { "MOUSE_RIGHT", MOUSE_BUTTON_RIGHT },
        { "MOUSE_MIDDLE", MOUSE_BUTTON_MIDDLE },
    };

    for (const auto& [entry_name, key] : c_NAMES)
    {
        if (name == entry_name)
        {
            return key;
        }
    }
    return -1;
}

inline void LoadScript(FState& state)
{
    state.m_bLoaded = true;

    const char* path = std::getenv("RAYWAVES_AGENT_INPUT");
    if (path == nullptr || path[0] == '\0')
    {
        return;
    }

    std::ifstream file(path);
    if (!file.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        const std::string_view view = TrimInput(line);
        if (view.empty() || view.front() == '#')
        {
            continue;
        }

        std::vector<std::string_view> parts;
        size_t start = 0;
        while (start <= view.size())
        {
            const size_t comma = view.find(',', start);
            const size_t end = (comma == std::string_view::npos) ? view.size() : comma;
            parts.push_back(TrimInput(view.substr(start, end - start)));
            if (comma == std::string_view::npos)
            {
                break;
            }
            start = comma + 1;
        }

        if (parts.size() < 3)
        {
            continue;
        }

        const int frame = std::atoi(std::string(parts[0]).c_str());
        if (parts[1] == "mouse" || parts[1] == "MOUSE")
        {
            if (parts.size() >= 4)
            {
                state.m_MouseEvents.push_back({ frame,
                    { static_cast<float>(std::atof(std::string(parts[2]).c_str())),
                        static_cast<float>(std::atof(std::string(parts[3]).c_str())) } });
            }
            continue;
        }

        const int key = ParseKeyName(parts[1]);
        if (key < 0)
        {
            continue;
        }

        std::string action(parts[2]);
        for (char& character : action)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        const bool b_down = (action == "down" || action == "1" || action == "true");

        state.m_Events.push_back({ frame, key, b_down });
    }

    std::stable_sort(state.m_Events.begin(), state.m_Events.end(),
        [](const FInputEvent& left, const FInputEvent& right) { return left.m_Frame < right.m_Frame; });
    std::stable_sort(state.m_MouseEvents.begin(), state.m_MouseEvents.end(),
        [](const FMouseEvent& left, const FMouseEvent& right) { return left.m_Frame < right.m_Frame; });

    state.m_bEnabled = !state.m_Events.empty() || !state.m_MouseEvents.empty();
}

inline FState& State()
{
    static FState state;
    return state;
}

inline void Reset()
{
    FState& state = State();
    if (!state.m_bLoaded)
    {
        return;
    }
    state.m_Frame = -1;
    state.m_EventCursor = 0;
    state.m_MouseCursor = 0;
    state.m_Down.clear();
    state.m_Pressed.clear();
    state.m_Released.clear();
    state.m_bHasMousePosition = false;
}

inline void BeginFrame()
{
    FState& state = State();
    if (!state.m_bLoaded)
    {
        LoadScript(state);
    }
    if (!state.m_bEnabled)
    {
        return;
    }

    ++state.m_Frame;
    state.m_Pressed.clear();
    state.m_Released.clear();

    while (state.m_EventCursor < state.m_Events.size() && state.m_Events[state.m_EventCursor].m_Frame <= state.m_Frame)
    {
        const FInputEvent& event = state.m_Events[state.m_EventCursor];
        const bool b_was_down = state.m_Down[event.m_Key];
        state.m_Down[event.m_Key] = event.m_bDown;
        if (event.m_bDown && !b_was_down)
        {
            state.m_Pressed.insert(event.m_Key);
        }
        else if (!event.m_bDown && b_was_down)
        {
            state.m_Released.insert(event.m_Key);
        }
        ++state.m_EventCursor;
    }

    while (state.m_MouseCursor < state.m_MouseEvents.size() &&
        state.m_MouseEvents[state.m_MouseCursor].m_Frame <= state.m_Frame)
    {
        state.m_MousePosition = state.m_MouseEvents[state.m_MouseCursor].m_Position;
        state.m_bHasMousePosition = true;
        ++state.m_MouseCursor;
    }
}

inline bool b_AgentMode()
{
    FState& state = State();
    if (!state.m_bLoaded)
    {
        LoadScript(state);
    }
    return state.m_bEnabled;
}
} // namespace input_detail

inline bool IsKeyDown(int key)
{
    if (input_detail::b_AgentMode())
    {
        const auto found = input_detail::State().m_Down.find(key);
        return found != input_detail::State().m_Down.end() && found->second;
    }
    return ::IsKeyDown(key);
}

inline bool IsKeyUp(int key)
{
    if (input_detail::b_AgentMode())
    {
        const auto found = input_detail::State().m_Down.find(key);
        return found == input_detail::State().m_Down.end() || !found->second;
    }
    return ::IsKeyUp(key);
}

inline bool IsKeyPressed(int key)
{
    if (input_detail::b_AgentMode())
    {
        return input_detail::State().m_Pressed.contains(key);
    }
    return ::IsKeyPressed(key);
}

inline bool IsKeyReleased(int key)
{
    if (input_detail::b_AgentMode())
    {
        return input_detail::State().m_Released.contains(key);
    }
    return ::IsKeyReleased(key);
}

inline bool IsMouseButtonDown(int button)
{
    if (input_detail::b_AgentMode())
    {
        const auto found = input_detail::State().m_Down.find(button);
        return found != input_detail::State().m_Down.end() && found->second;
    }
    return ::IsMouseButtonDown(button);
}

inline bool IsMouseButtonPressed(int button)
{
    if (input_detail::b_AgentMode())
    {
        return input_detail::State().m_Pressed.contains(button);
    }
    return ::IsMouseButtonPressed(button);
}

inline bool IsMouseButtonReleased(int button)
{
    if (input_detail::b_AgentMode())
    {
        return input_detail::State().m_Released.contains(button);
    }
    return ::IsMouseButtonReleased(button);
}

inline Vector2 GetMousePosition()
{
    if (input_detail::b_AgentMode() && input_detail::State().m_bHasMousePosition)
    {
        return input_detail::State().m_MousePosition;
    }
    return ::GetMousePosition();
}

inline float GetMouseX()
{
    return GetMousePosition().x;
}

inline float GetMouseY()
{
    return GetMousePosition().y;
}

// Named input actions on top of the replay-aware key queries.
//
// Actions group several physical keys so gameplay code reads intent instead of
// raw key codes. Lookups route through rw::IsKeyDown/Pressed/Released, so agent
// replay scripts (RAYWAVES_AGENT_INPUT) drive actions exactly like real input.
class ActionMap
{
public:
    // Adds one key to an action. Binding the same key twice is allowed; the
    // action simply reports the union of its keys.
    void Bind(std::string_view action, int key)
    {
        if (action.empty())
        {
            return;
        }
        m_Bindings[std::string(action)].push_back(key);
    }

    void Bind(std::string_view action, std::initializer_list<int> keys)
    {
        if (action.empty() || keys.size() == 0)
        {
            return;
        }
        std::vector<int>& binding = m_Bindings[std::string(action)];
        binding.insert(binding.end(), keys.begin(), keys.end());
    }

    bool b_Down(std::string_view action) const
    {
        return b_AnyKey(action, [](int key) { return rw::IsKeyDown(key); });
    }

    bool b_Pressed(std::string_view action) const
    {
        return b_AnyKey(action, [](int key) { return rw::IsKeyPressed(key); });
    }

    bool b_Released(std::string_view action) const
    {
        return b_AnyKey(action, [](int key) { return rw::IsKeyReleased(key); });
    }

    bool b_IsBound(std::string_view action) const { return FindKeys(action) != nullptr; }

    // Total number of key bindings across all actions, counting duplicates.
    std::size_t BindingCount() const
    {
        std::size_t count = 0;
        for (const auto& entry : m_Bindings)
        {
            count += entry.second.size();
        }
        return count;
    }

    void Clear() { m_Bindings.clear(); }

    void Unbind(std::string_view action)
    {
        if (action.empty())
        {
            return;
        }
        m_Bindings.erase(std::string(action));
    }

private:
    template <typename TPredicate>
    bool b_AnyKey(std::string_view action, TPredicate predicate) const
    {
        const std::vector<int>* keys = FindKeys(action);
        if (keys == nullptr)
        {
            return false;
        }
        for (const int key : *keys)
        {
            if (predicate(key))
            {
                return true;
            }
        }
        return false;
    }

    const std::vector<int>* FindKeys(std::string_view action) const
    {
        if (action.empty())
        {
            return nullptr;
        }
        const auto found = m_Bindings.find(std::string(action));
        return found == m_Bindings.end() ? nullptr : &found->second;
    }

    std::unordered_map<std::string, std::vector<int>> m_Bindings;
};
} // namespace rw
