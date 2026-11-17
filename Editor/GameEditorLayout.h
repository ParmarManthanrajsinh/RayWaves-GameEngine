#pragma once
#include "rlImGui/extras/IconsFontAwesome6.h"
#include <cstring>
#include <imgui.h>
#include <string>

inline void LoadEditorDefaultIni()
{
    // Disable saving to file
    ImGui::GetIO().IniFilename = nullptr;

    std::string ini = std::string(R"(
[Window][WindowOverViewport_11111111]
Pos=0,25
Size=1280,695
Collapsed=0

[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Window][)") +
        ICON_FA_IMAGE + R"( Viewport]
Pos=280,25
Size=660,470
Collapsed=0
DockId=0x00000001,0

[Window][)" +
        ICON_FA_FOLDER_TREE + R"( FileSystem]
Pos=0,375
Size=280,345
Collapsed=0
DockId=0x00000005,0

[Window][)" +
        ICON_FA_SITEMAP + R"( Scene]
Pos=0,25
Size=280,350
Collapsed=0
DockId=0x00000009,0

[Window][)" +
        ICON_FA_SLIDERS + R"( Inspector]
Pos=980,25
Size=300,695
Collapsed=0
DockId=0x00000003,0

[Window][)" +
        ICON_FA_FILE_EXPORT + R"( Export]
Pos=141,134
Size=695,748
Collapsed=0

[Window][)" +
        ICON_FA_GEARS + R"( Scene Settings]
Pos=65,116
Size=564,222
Collapsed=0

[Window][Performance Overlay]
Pos=10,140
Size=220,182
Collapsed=0

[Window][Message Log]
Pos=10,140
Size=220,182
Collapsed=0

[Window][)" +
        ICON_FA_TERMINAL + R"( Terminal]
Pos=280,495
Size=660,225
Collapsed=0
DockId=0x00000004,0

[Window][##MainMenuBar]
Size=1280,25
Collapsed=0

[Docking][Data]
DockSpace     ID=0x08BD597D Window=0x1BBC0F80 Pos=0,25 Size=1280,695 Split=X
  DockNode    ID=0x00000008 Parent=0x08BD597D SizeRef=280,695 Split=Y
    DockNode  ID=0x00000009 Parent=0x00000008 SizeRef=280,350
    DockNode  ID=0x00000005 Parent=0x00000008 SizeRef=280,345
  DockNode    ID=0x0000000B Parent=0x08BD597D SizeRef=1000,695 Split=X
    DockNode  ID=0x00000002 Parent=0x0000000B SizeRef=660,695 Split=Y
      DockNode  ID=0x00000001 Parent=0x00000002 SizeRef=660,470 CentralNode=1
      DockNode  ID=0x00000004 Parent=0x00000002 SizeRef=660,225
    DockNode  ID=0x00000003 Parent=0x0000000B SizeRef=300,695
)";

    ImGui::LoadIniSettingsFromMemory(ini.c_str(), ini.size());
}

// Core Shaders for raylib
constexpr std::string_view c_OPAQUE_VERT_SHADER_SRC = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
out vec2 fragTexCoord;
out vec4 fragColor;
uniform mat4 mvp;
void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

constexpr std::string_view c_OPAQUE_FRAG_SHADER_SRC = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
void main()
{
    vec4 c = texture(texture0, fragTexCoord) * fragColor;
    finalColor = vec4(c.rgb, 1.0);
}
)";

inline Shader LoadOpaqueShader()
{
    return LoadShaderFromMemory(c_OPAQUE_VERT_SHADER_SRC.data(), c_OPAQUE_FRAG_SHADER_SRC.data());
}
