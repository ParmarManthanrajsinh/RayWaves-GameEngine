add_executable(raywaves_benchmarks
    main.cpp
    statebag_bench.cpp
    profiler_bench.cpp
    asset_resolver_bench.cpp
    rw_bench.cpp
)

set_target_properties(raywaves_benchmarks PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_include_directories(raywaves_benchmarks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Engine
)

target_link_libraries(raywaves_benchmarks PRIVATE Engine Threads::Threads)
add_dependencies(raywaves_benchmarks stage_raylib)
raywaves_set_origin_rpath(raywaves_benchmarks)

add_custom_command(TARGET raywaves_benchmarks POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:raylib>
            $<TARGET_FILE_DIR:raywaves_benchmarks>
)
if(UNIX)
    add_custom_command(TARGET raywaves_benchmarks POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_SONAME_FILE:raylib>" "$<TARGET_FILE_DIR:raywaves_benchmarks>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_LINKER_FILE:raylib>" "$<TARGET_FILE_DIR:raywaves_benchmarks>/libraylib.so"
    )
endif()

add_custom_target(bench
    COMMAND $<TARGET_FILE:raywaves_benchmarks>
    DEPENDS raywaves_benchmarks
    COMMENT "Running RayWaves benchmarks"
)
