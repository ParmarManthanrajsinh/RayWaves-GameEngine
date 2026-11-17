#include "Scene.h"
#include "AssetResolver.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>

namespace
{
std::string_view Trim(std::string_view value)
{
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
    {
        return {};
    }

    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, (end - start) + 1);
}

std::vector<std::string_view> Split(std::string_view value, char delimiter)
{
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start <= value.size())
    {
        size_t end = value.find(delimiter, start);
        if (end == std::string_view::npos)
        {
            end = value.size();
        }

        parts.push_back(Trim(value.substr(start, end - start)));
        if (end == value.size())
        {
            break;
        }
        start = end + 1;
    }
    return parts;
}

float ParseFloat(std::string_view text, float fallback)
{
    float value = fallback;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    (void)end;
    return error == std::errc{} ? value : fallback;
}

int ParseInt(std::string_view text, int fallback)
{
    int value = fallback;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    (void)end;
    return error == std::errc{} ? value : fallback;
}

uint32_t ParseUInt(std::string_view text, uint32_t fallback)
{
    uint32_t value = fallback;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    (void)end;
    return error == std::errc{} ? value : fallback;
}

Vector2 ParseVector2(std::string_view value, Vector2 fallback)
{
    const std::vector<std::string_view> parts = Split(value, ',');
    if (parts.size() < 2)
    {
        return fallback;
    }
    return { ParseFloat(parts[0], fallback.x), ParseFloat(parts[1], fallback.y) };
}

Rectangle ParseRectangle(std::string_view value)
{
    const std::vector<std::string_view> parts = Split(value, ',');
    if (parts.size() < 4)
    {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    return { ParseFloat(parts[0], 0.0f), ParseFloat(parts[1], 0.0f), ParseFloat(parts[2], 0.0f),
        ParseFloat(parts[3], 0.0f) };
}

Color ParseColor(std::string_view value)
{
    const std::vector<std::string_view> parts = Split(value, ',');
    if (parts.size() < 4)
    {
        return WHITE;
    }

    const auto channel = [](std::string_view text) -> unsigned char
    {
        const int value = std::clamp(ParseInt(text, 255), 0, 255);
        return static_cast<unsigned char>(value);
    };
    return Color{ channel(parts[0]), channel(parts[1]), channel(parts[2]), channel(parts[3]) };
}

// Unknown keys are ignored so newer/older scene files still load. Bad values
// keep the previous field, which makes hand-editing forgiving.
void ApplyKey(t_SceneObject& object, std::string_view key, std::string_view value)
{
    if (key == "id")
    {
        object.m_Id = ParseUInt(value, object.m_Id);
    }
    else if (key == "name")
    {
        object.m_Name = std::string(value);
    }
    else if (key == "texture")
    {
        object.m_TexturePath = std::string(value);
    }
    else if (key == "position")
    {
        object.m_Position = ParseVector2(value, object.m_Position);
    }
    else if (key == "rotation")
    {
        object.m_Rotation = ParseFloat(value, object.m_Rotation);
    }
    else if (key == "scale")
    {
        object.m_Scale = ParseVector2(value, object.m_Scale);
    }
    else if (key == "source")
    {
        object.m_Source = ParseRectangle(value);
    }
    else if (key == "tint")
    {
        object.m_Tint = ParseColor(value);
    }
    else if (key == "layer")
    {
        object.m_Layer = ParseInt(value, object.m_Layer);
    }
    else if (key == "visible")
    {
        object.m_bVisible = ParseInt(value, object.m_bVisible ? 1 : 0) != 0;
    }
}
} // namespace

std::string Scene::ToFileKey(std::string_view key)
{
    std::string result;
    result.reserve(key.size());
    for (const char character : key)
    {
        if (std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_' || character == '-')
        {
            result += character;
        }
        else
        {
            result += '_';
        }
    }
    return result;
}

t_SceneObject& Scene::AddObject(std::string_view name)
{
    t_SceneObject object;
    object.m_Id = m_NextId++;
    object.m_Name = name.empty() ? "Object" : std::string(name);
    m_Objects.push_back(std::move(object));
    m_bDirty = true;
    return m_Objects.back();
}

t_SceneObject* Scene::DuplicateObject(uint32_t object_id)
{
    const t_SceneObject* source = FindObject(object_id);
    if (source == nullptr)
    {
        return nullptr;
    }

    t_SceneObject copy = *source;
    copy.m_Id = m_NextId++;
    // Never share GPU handles between objects; the copy reloads lazily.
    copy.m_Texture = {};
    copy.m_bTextureMissing = false;
    m_Objects.push_back(std::move(copy));
    m_bDirty = true;
    return &m_Objects.back();
}

bool Scene::b_RemoveObject(uint32_t object_id)
{
    auto object_it =
        std::ranges::find_if(m_Objects, [object_id](const t_SceneObject& object) { return object.m_Id == object_id; });
    if (object_it == m_Objects.end())
    {
        return false;
    }

    UnloadTexture(*object_it);
    m_Objects.erase(object_it);
    m_bDirty = true;
    return true;
}

t_SceneObject* Scene::FindObject(uint32_t object_id)
{
    for (t_SceneObject& object : m_Objects)
    {
        if (object.m_Id == object_id)
        {
            return &object;
        }
    }
    return nullptr;
}

const t_SceneObject* Scene::FindObject(uint32_t object_id) const
{
    for (const t_SceneObject& object : m_Objects)
    {
        if (object.m_Id == object_id)
        {
            return &object;
        }
    }
    return nullptr;
}

bool Scene::b_EnsureTexture(t_SceneObject& object)
{
    if (IsTextureValid(object.m_Texture))
    {
        return true;
    }
    if (object.m_TexturePath.empty() || object.m_bTextureMissing)
    {
        return false;
    }

    object.m_Texture = LoadTexture(AssetResolver::Resolve(object.m_TexturePath).c_str());
    if (!IsTextureValid(object.m_Texture))
    {
        object.m_Texture = {};
        object.m_bTextureMissing = true;
        return false;
    }
    object.m_bTextureMissing = false;
    return true;
}

void Scene::UnloadTexture(t_SceneObject& object)
{
    if (IsTextureValid(object.m_Texture))
    {
        ::UnloadTexture(object.m_Texture);
    }
    object.m_Texture = {};
}

void Scene::ReloadTexture(uint32_t object_id)
{
    t_SceneObject* object = FindObject(object_id);
    if (object == nullptr)
    {
        return;
    }

    UnloadTexture(*object);
    object->m_bTextureMissing = false;
    b_EnsureTexture(*object);
    m_bDirty = true;
}

void Scene::Draw()
{
    std::vector<t_SceneObject*> ordered;
    ordered.reserve(m_Objects.size());
    for (t_SceneObject& object : m_Objects)
    {
        if (object.m_bVisible)
        {
            ordered.push_back(&object);
        }
    }

    std::ranges::stable_sort(ordered, {}, [](const t_SceneObject* object) { return object->m_Layer; });

    for (t_SceneObject* object : ordered)
    {
        if (!b_EnsureTexture(*object))
        {
            // No texture yet: keep the object visible while the user works on it.
            const Rectangle placeholder{ object->m_Position.x, object->m_Position.y, 32.0f * object->m_Scale.x,
                32.0f * object->m_Scale.y };
            DrawRectangleRec(placeholder, Fade(object->m_Tint, 0.25f));
            DrawRectangleLinesEx(placeholder, 1.0f, Fade(object->m_Tint, 0.9f));
            continue;
        }

        const Rectangle source = (object->m_Source.width > 0.0f && object->m_Source.height > 0.0f)
            ? object->m_Source
            : Rectangle{ 0.0f, 0.0f, static_cast<float>(object->m_Texture.width),
                  static_cast<float>(object->m_Texture.height) };

        const Rectangle destination{ object->m_Position.x, object->m_Position.y, source.width * object->m_Scale.x,
            source.height * object->m_Scale.y };

        DrawTexturePro(object->m_Texture, source, destination, { 0.0f, 0.0f }, object->m_Rotation, object->m_Tint);
    }
}

void Scene::Clear()
{
    for (t_SceneObject& object : m_Objects)
    {
        UnloadTexture(object);
    }
    m_Objects.clear();
    m_NextId = 1;
    m_bDirty = false;
}

bool Scene::b_SaveToFile(std::string_view path)
{
    if (path.empty())
    {
        return false;
    }

    const std::filesystem::path file_path(path);
    const std::filesystem::path parent = file_path.parent_path();
    if (!parent.empty())
    {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
    }

    std::ofstream file(file_path);
    if (!file.is_open())
    {
        return false;
    }

    file << "[scene]\n";
    file << "version=1\n";

    for (const t_SceneObject& object : m_Objects)
    {
        file << "\n[object]\n";
        file << "id=" << object.m_Id << "\n";
        file << "name=" << object.m_Name << "\n";
        file << "texture=" << object.m_TexturePath << "\n";
        file << "position=" << object.m_Position.x << "," << object.m_Position.y << "\n";
        file << "rotation=" << object.m_Rotation << "\n";
        file << "scale=" << object.m_Scale.x << "," << object.m_Scale.y << "\n";
        file << "source=" << object.m_Source.x << "," << object.m_Source.y << "," << object.m_Source.width << ","
             << object.m_Source.height << "\n";
        file << "tint=" << static_cast<int>(object.m_Tint.r) << "," << static_cast<int>(object.m_Tint.g) << ","
             << static_cast<int>(object.m_Tint.b) << "," << static_cast<int>(object.m_Tint.a) << "\n";
        file << "layer=" << object.m_Layer << "\n";
        file << "visible=" << (object.m_bVisible ? 1 : 0) << "\n";
    }

    file.close();
    m_bDirty = false;
    return true;
}

bool Scene::b_LoadFromFile(std::string_view path)
{
    if (path.empty())
    {
        return false;
    }

    std::ifstream file{ std::string(path) };
    if (!file.is_open())
    {
        return false;
    }

    Clear();

    t_SceneObject current;
    bool b_has_current = false;

    const auto flush = [this, &current, &b_has_current]()
    {
        if (!b_has_current)
        {
            return;
        }
        if (current.m_Id == 0)
        {
            current.m_Id = m_NextId;
        }
        m_NextId = std::max(m_NextId, current.m_Id + 1);
        m_Objects.push_back(std::move(current));
        current = t_SceneObject{};
        b_has_current = false;
    };

    std::string line;
    while (std::getline(file, line))
    {
        const std::string_view view = Trim(line);
        if (view.empty() || view == "[scene]")
        {
            continue;
        }

        if (view == "[object]")
        {
            flush();
            b_has_current = true;
            continue;
        }

        const size_t equal = view.find('=');
        if (equal == std::string_view::npos || !b_has_current)
        {
            continue;
        }

        const std::string_view key = Trim(view.substr(0, equal));
        const std::string_view value = Trim(view.substr(equal + 1));
        ApplyKey(current, key, value);
    }

    flush();
    m_bDirty = false;
    return true;
}
