#include "support/bench.hpp"

#include "Scene.h"

#include <cstdint>

namespace
{
constexpr int c_ObjectCount = 10000;
constexpr int c_Iterations = 1000;

// Mixed layers and visibility so every rebuild does real work and skips part of
// the list, matching a busy editor scene.
Scene s_MakeScene()
{
    Scene scene;
    for (int index = 0; index < c_ObjectCount; ++index)
    {
        t_SceneObject& object = scene.AddObject("Object");
        object.m_Layer = (index * 37) % 64;
        object.m_bVisible = (index % 7) != 0;
    }
    scene.ClearDirty();
    return scene;
}

// Keeps the measured reads observable so they are not optimized away.
volatile uint32_t s_Sink = 0;
} // namespace

BENCHMARK_CASE("Scene::draw order rebuild (10k objects)", c_Iterations)
{
    static Scene scene = s_MakeScene();

    for (int i = 0; i < c_Iterations; ++i)
    {
        scene.MarkDirty();
        s_Sink = static_cast<uint32_t>(scene.GetDrawOrder().size());
    }
}

BENCHMARK_CASE("Scene::draw order cached read (10k objects)", c_Iterations)
{
    static Scene scene = s_MakeScene();
    // First read pays the rebuild; the timed loop only returns the member
    // reference, so this measures the allocation-free O(1) cached path.
    (void)scene.GetDrawOrder();

    uint64_t checksum = 0;
    for (int i = 0; i < c_Iterations; ++i)
    {
        checksum += scene.GetDrawOrder().size();
    }
    s_Sink = static_cast<uint32_t>(checksum);
}
