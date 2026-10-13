#pragma once
#include <cstdint>
#include <cstddef>

#define SPIRE_STATE(type, ...)
#define SPIRE_SNAPSHOT(...)
#define SPIRE_STATIC(...)

#if defined(__CUDACC__)
#define SPIRE_FN __host__ __device__
#else
#define SPIRE_FN
#endif

#if defined(__CUDA_ARCH__)
#define SPIRE_ASSERT(condition) do { if (!(condition)) { asm("trap;"); } } while (false)
#else
#define SPIRE_ASSERT(condition) do { if (!(condition)) { __builtin_trap(); } } while (false)
#endif

namespace spire {
SPIRE_FN constexpr int min(int a, int b) { return a < b ? a : b; }
SPIRE_FN constexpr int max(int a, int b) { return a > b ? a : b; }
SPIRE_FN inline int floor_int(float value) {
    const int truncated = static_cast<int>(value);
    return truncated - (value < static_cast<float>(truncated) ? 1 : 0);
}
}
