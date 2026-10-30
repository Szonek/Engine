#pragma once
#include <cstdint>

namespace project_c
{
// SplitMix64 (Steele, Lea and Flood, 2014): a small, fast generator whose numbers depend on the seed only, never on the
// compiler or its standard library (std::uniform_real_distribution gives different numbers on MSVC and GCC), so the
// same seed gives the same level on every platform.
class Random
{
public:
    explicit Random(std::uint64_t seed)
        : state_(seed)
    {
    }

    std::uint64_t next_u64()
    {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    // in [0, 1): the top 24 bits, so every value is exactly a float
    float next_float()
    {
        return static_cast<float>(next_u64() >> 40) * (1.0f / 16777216.0f);
    }

    // in [min, max)
    float range(float min, float max)
    {
        return min + (max - min) * next_float();
    }

private:
    std::uint64_t state_;
};
}  // namespace project_c
