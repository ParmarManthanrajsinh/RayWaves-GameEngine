#include<cstring>
#include<imgui.h>

// Optional: disable saving to file
// ImGui::GetIO().IniFilename = nullptr;

// Default docking layout for the editor, loaded on startup
static constexpr std::string_view s_cEDITOR_DEFAULT_INI = 
R"(
[Window][WindowOverViewport_11111111]
Pos=0,0
Size=1280,720
Collapsed=0

[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Window][Scene]
Pos=0,0
Size=972,490
Collapsed=0
DockId=0x00000005,0

[Window][Map Selection]
Pos=974,0
Size=306,720
Collapsed=0
DockId=0x00000002,0

[Window][Export]
Pos=0,0
Size=972,490
Collapsed=0
DockId=0x00000005,2

[Window][Scene Settings]
Pos=0,0
Size=972,490
Collapsed=0
DockId=0x00000005,1

[Window][Performance Overlay]
Pos=10,140
Size=220,182
Collapsed=0

[Window][Debug Console]
Pos=0,492
Size=972,228
Collapsed=0
DockId=0x00000006,0

[Docking][Data]
DockSpace       ID=0x08BD597D Window=0x1BBC0F80 Pos=0,0 Size=1280,720 Split=Y Selected=0xE601B12F
  DockNode      ID=0x00000003 Parent=0x08BD597D SizeRef=1280,520 Split=X Selected=0xE601B12F
    DockNode    ID=0x00000001 Parent=0x00000003 SizeRef=972,720 Split=Y Selected=0xE601B12F
      DockNode  ID=0x00000005 Parent=0x00000001 SizeRef=972,490 CentralNode=1 Selected=0xE601B12F
      DockNode  ID=0x00000006 Parent=0x00000001 SizeRef=972,228 HiddenTabBar=1 Selected=0x9F51CE4F
    DockNode    ID=0x00000002 Parent=0x00000003 SizeRef=306,720 Selected=0x9D14B58E
  DockNode      ID=0x00000004 Parent=0x08BD597D SizeRef=1280,198 Selected=0x9C2B5678
)";

inline void LoadEditorDefaultIni()
{
    ImGui::LoadIniSettingsFromMemory
    (
        s_cEDITOR_DEFAULT_INI.data(), strlen(s_cEDITOR_DEFAULT_INI.data())
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