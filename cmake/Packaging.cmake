# The editor reads this template at runtime to generate each project's
# .raywaves/CMakeLists.txt. Staging it next to the executables keeps dev
# builds and packaged builds on the exact same recipe.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/packaging")
configure_file(
    "${CMAKE_SOURCE_DIR}/packaging/Project.CMakeLists.in"
    "${CMAKE_BINARY_DIR}/packaging/Project.CMakeLists.in"
    COPYONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/packaging/Project.AGENTS.md.in"
    "${CMAKE_BINARY_DIR}/packaging/Project.AGENTS.md.in"
    COPYONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/packaging/Project.SKILL.md.in"
    "${CMAKE_BINARY_DIR}/packaging/Project.SKILL.md.in"
    COPYONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/packaging/Project.rwcli-reference.md.in"
    "${CMAKE_BINARY_DIR}/packaging/Project.rwcli-reference.md.in"
    COPYONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/packaging/Project.clangd.in"
    "${CMAKE_BINARY_DIR}/packaging/Project.clangd.in"
    COPYONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/packaging/Project.vscode.c_cpp_properties.json.in"
    "${CMAKE_BINARY_DIR}/packaging/Project.vscode.c_cpp_properties.json.in"
    COPYONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/packaging/Project.vscode.settings.json.in"
    "${CMAKE_BINARY_DIR}/packaging/Project.vscode.settings.json.in"
    COPYONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/packaging/Project.vscode.extensions.json.in"
    "${CMAKE_BINARY_DIR}/packaging/Project.vscode.extensions.json.in"
    COPYONLY
)

if(WIN32)
    add_custom_target(export_package
        COMMAND powershell -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/packaging/package.ps1" -OutputDir dist
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Packaging distribution via packaging/package.ps1"
    )
else()
    add_custom_target(export_package
        COMMAND bash "${CMAKE_SOURCE_DIR}/packaging/package.sh" --output-dir dist
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Packaging distribution via packaging/package.sh"
    )
endif()
