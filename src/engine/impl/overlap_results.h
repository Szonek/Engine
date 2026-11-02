#pragma once
#include "engine.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace engine
{
// a game object an overlap query found
struct OverlapHit
{
    engine_game_object_t go = ENGINE_INVALID_GAME_OBJECT_ID;
    // from the query's center to the collider's surface (negative when the center is inside it)
    float distance = 0.0f;
};

// The overlap query's result, nearest first (ties by handle, so the order never depends on Bullet's): the nearest
// `out.size()` game objects are written to `out`; returns how many were found, which can be more (like Unity's
// Physics.OverlapSphereNonAlloc).
inline std::size_t write_nearest_first(std::vector<OverlapHit>& hits, std::span<engine_game_object_t> out)
{
    std::sort(hits.begin(), hits.end(), [](const OverlapHit& a, const OverlapHit& b)
        {
            return a.distance != b.distance ? a.distance < b.distance : a.go < b.go;
        });
    const std::size_t written = std::min(hits.size(), out.size());
    for (std::size_t i = 0; i < written; i++)
    {
        out[i] = hits[i].go;
    }
    return hits.size();
}
}  // namespace engine
