add_executable(raywaves_benchmarks
    main.cpp
    statebag_bench.cpp
    profiler_bench.cpp
    asset_resolver_bench.cpp
)

set_target_properties(raywaves_benchmarks PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_include_directories(raywaves_benchmarks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
)

target_link_libraries(raywaves_benchmarks PRIVATE Engine)
add_dependencies(raywaves_benchmarks stage_raylib)

add_custom_command(TARGET raywaves_benchmarks POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:raylib>
            $<TARGET_FILE_DIR:raywaves_benchmarks>
)

add_custom_target(bench
    COMMAND $<TARGET_FILE:raywaves_benchmarks>
    DEPENDS raywaves_benchmarks
    COMMENT "Running RayWaves benchmarks"
)
