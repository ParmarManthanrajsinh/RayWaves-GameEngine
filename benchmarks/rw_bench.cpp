#include "support/bench.hpp"

#include "rw.h"

namespace
{
constexpr int k_Entities = 1000;

class BenchLevel : public rw::Level
{
public:
    BenchLevel() : rw::Level("BenchLevel") {}

protected:
    void OnStart() override
    {
        for (int index = 0; index < k_Entities; ++index)
        {
            Spawn("Entity", { static_cast<float>(index), 0.0f });
        }
    }
};

BenchLevel& GetLevel()
{
    static BenchLevel level;
    return level;
}
} // namespace

BENCHMARK_CASE("rw::Level Spawn + editor registration", k_Entities)
{
    BenchLevel& level = GetLevel();
    level.ClearEntities();
    for (int index = 0; index < k_Entities; ++index)
    {
        level.Spawn("Entity", { static_cast<float>(index), 0.0f });
    }
}

BENCHMARK_CASE("rw::Entity transform write", k_Entities)
{
    BenchLevel& level = GetLevel();
    if (level.Entities().size() != static_cast<size_t>(k_Entities))
    {
        level.ClearEntities();
        for (int index = 0; index < k_Entities; ++index)
        {
            level.Spawn("Entity", { static_cast<float>(index), 0.0f });
        }
    }

    for (const auto& entity : level.Entities())
    {
        entity->position.x += 1.0f;
    }
}
