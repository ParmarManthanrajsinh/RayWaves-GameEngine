#include "FileAssociation.h"
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <filesystem>

std::string GetExecutablePath()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::filesystem::path(path).lexically_normal().string();
}

bool RegisterRayWavesFileAssociation()
{
    std::string exePath = GetExecutablePath();
    std::string exeQuoted = "\"" + exePath + "\" \"%1\"";

    HKEY hKey = nullptr;

    // .raywaves -> RayWaves.Project
    LONG result = RegCreateKeyExA(HKEY_CURRENT_USER,
        "Software\\Classes\\.raywaves",
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS) return false;
    RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)"RayWaves.Project",
        sizeof("RayWaves.Project"));
    RegCloseKey(hKey);

    // RayWaves.Project description
    result = RegCreateKeyExA(HKEY_CURRENT_USER,
        "Software\\Classes\\RayWaves.Project",
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS) return false;
    RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)"RayWaves Project",
        sizeof("RayWaves Project"));
    RegCloseKey(hKey);

    // DefaultIcon
    result = RegCreateKeyExA(HKEY_CURRENT_USER,
        "Software\\Classes\\RayWaves.Project\\DefaultIcon",
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS) return false;
    std::string iconPath = exePath + ",0";
    RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)iconPath.c_str(),
        static_cast<DWORD>(iconPath.size() + 1));
    RegCloseKey(hKey);

    // shell\open\command
    result = RegCreateKeyExA(HKEY_CURRENT_USER,
        "Software\\Classes\\RayWaves.Project\\shell\\open\\command",
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS) return false;
    RegSetValueExA(hKey, NULL, 0, REG_SZ, (const BYTE*)exeQuoted.c_str(),
        static_cast<DWORD>(exeQuoted.size() + 1));
    RegCloseKey(hKey);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return true;
}

bool IsRayWavesFileAssociationRegistered()
{
    HKEY hKey = nullptr;
    LONG result = RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Classes\\RayWaves.Project\\shell\\open\\command",
        0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) return false;

    char value[1024];
    DWORD valueSize = sizeof(value);
    result = RegQueryValueExA(hKey, NULL, NULL, NULL,
        (LPBYTE)value, &valueSize);
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) return false;

    std::string current = GetExecutablePath();
    std::string registered(value, valueSize - 1);

    // Compare by checking if registered command starts with quoted exe path
    std::string expected = "\"" + current + "\"";
    return registered.compare(0, expected.size(), expected) == 0;
}
