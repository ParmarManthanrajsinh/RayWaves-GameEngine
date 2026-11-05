# The editor reads this template at runtime to generate each project's
# .raywaves/CMakeLists.txt. Staging it next to the executables keeps dev
# builds and packaged builds on the exact same recipe.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/packaging")
configure_file(
    "${CMAKE_SOURCE_DIR}/packaging/Project.CMakeLists.in"
    "${CMAKE_BINARY_DIR}/packaging/Project.CMakeLists.in"
    COPYONLY
)

add_custom_target(export_package
    COMMAND powershell -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/packaging/package.ps1" -OutputDir dist
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Packaging distribution via packaging/package.ps1"
)
