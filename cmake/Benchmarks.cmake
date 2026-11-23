add_subdirectory(benchmarks)

# The scene draw-order benchmark is registered here rather than in
# benchmarks/CMakeLists.txt so this work package stays within its owned files.
target_sources(raywaves_benchmarks PRIVATE
    ${CMAKE_SOURCE_DIR}/benchmarks/SceneDrawOrder_bench.cpp
)
