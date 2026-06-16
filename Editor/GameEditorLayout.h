#include<cstring>
#include<string>
#include<imgui.h>
#include "rlImGui/extras/IconsFontAwesome6.h"

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

[Window][)") + ICON_FA_IMAGE + R"( Scene]
Pos=0,25
Size=966,478
Collapsed=1
DockId=0x00000001,0

[Window][)" + ICON_FA_MAP + R"( Map Selection]
Pos=968,25
Size=312,695
Collapsed=0
DockId=0x00000003,0

[Window][)" + ICON_FA_FILE_EXPORT + R"( Export]
Pos=141,134
Size=695,748
Collapsed=0

[Window][)" + ICON_FA_GEARS + R"( Scene Settings]
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

[Window][)" + ICON_FA_TERMINAL + R"( Console]
Pos=0,505
Size=966,215
Collapsed=0
DockId=0x00000004,0

[Window][##MainMenuBar]
Size=1280,25
Collapsed=0

[Docking][Data]
DockSpace     ID=0x08BD597D Window=0x1BBC0F80 Pos=0,25 Size=1280,695 Split=X
  DockNode    ID=0x00000002 Parent=0x08BD597D SizeRef=966,695 Split=Y Selected=0xE53DD001
    DockNode  ID=0x00000001 Parent=0x00000002 SizeRef=951,478 CentralNode=1 Selected=0xE53DD001
    DockNode  ID=0x00000004 Parent=0x00000002 SizeRef=951,215 Selected=0x8509AE9E
  DockNode    ID=0x00000003 Parent=0x08BD597D SizeRef=312,695 Selected=0x38B9FCCC
)";

    ImGui::LoadIniSettingsFromMemory
    (
        ini.c_str(), ini.size()
    );
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
    return LoadShaderFromMemory
    (
        c_OPAQUE_VERT_SHADER_SRC.data(), c_OPAQUE_FRAG_SHADER_SRC.data()
    );
}