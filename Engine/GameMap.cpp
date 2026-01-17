#include "GameMap.h"

GameMap::GameMap()
	: m_MapName("DefaultMap") {}

GameMap::GameMap(const std::string& map_name)
	: m_MapName(map_name) {}

void GameMap::Initialize()
{
    // Initialization code for the game map
}

void GameMap::Update(float delta_time)
{
    // Update logic for the game map
}

void GameMap::Draw()
{
    // Drawing logic for the game map
}

void GameMap::SetMapName(const std::string& map_name)
{
    m_MapName = map_name;
}

std::string GameMap::GetMapName() const
{
    return m_MapName;
}

void GameMap::SetSceneBounds(float width, float height)
{
    m_SceneWidth = width;
    m_SceneHeight = height;
}

Vector2 GameMap::GetSceneBounds() const
{
	return Vector2(m_SceneWidth, m_SceneHeight);
}

void GameMap::SetTargetFPS(int fps)
{
    ::SetTargetFPS(fps);
	m_TargetFPS = fps;
}

int GameMap::GetTargetFPS() const
{
    return m_TargetFPS;
}

void GameMap::SetTransitionCallback
(
    std::function<void(std::string_view, bool)> cb
)
{
    m_TransitionCallback = std::move(cb);
}

void GameMap::RequestGotoMap(std::string_view map_id, bool force_reload)
{
    if (m_TransitionCallback)
    {
        m_TransitionCallback(map_id, force_reload);
    }
    else
    {
		std::println(std::cerr, "Transition callback not set!");
    }
}
