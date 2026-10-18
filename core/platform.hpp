#pragma once
#include <cstdint>
#include <cstddef>

#define SPIRE_STATE(type, ...)
#define SPIRE_RULE(...)
#define SPIRE_WAITING_RULE(...)
#define SPIRE_NOT_APPLICABLE(...)
#define SPIRE_SNAPSHOT(...)
#define SPIRE_STATIC(...)
#define SPIRE_STATIC_STATE(type, ...)

#define SPIRE_INLINE __attribute__((always_inline))
#if defined(__CUDA_ARCH__)
#define SPIRE_CONSTANT __constant__
#define SPIRE_STORAGE __device__
#else
#define SPIRE_CONSTANT
#define SPIRE_STORAGE const
#endif

#if defined(__CUDACC__)
#define SPIRE_FN __host__ __device__
#else
#define SPIRE_FN
#endif

#if defined(__CUDA_ARCH__)
#define SPIRE_ASSERT(condition) do { if (!(condition)) { ::spire::device_fault(::spire::assertion_id(__FILE__, __LINE__)); } } while (false)
#else
#define SPIRE_ASSERT(condition) do { if (!(condition)) { __builtin_trap(); } } while (false)
#endif

namespace spire {
template<class T> struct ReadOnly { const T value; };
template<int Alignment> struct StateWord;
template<> struct StateWord<1> { using type = uint8_t; };
template<> struct StateWord<4> { using type = uint32_t; };
SPIRE_FN SPIRE_INLINE constexpr uint32_t assertion_id(const char* file, uint32_t line) {
    uint32_t hash = 2166136261u;
    while (*file) hash = (hash ^ static_cast<uint8_t>(*file++)) * 16777619u;
    hash = (hash ^ line) * 16777619u;
    return hash == 0 ? 1 : hash;
}
SPIRE_FN SPIRE_INLINE inline void record_fault(uint32_t& fault, uint32_t site) {
    if (fault == 0) fault = site;
}
#if defined(__CUDA_ARCH__)
SPIRE_FN uint32_t*& device_fault_target();
[[noreturn]] SPIRE_FN SPIRE_INLINE inline void device_fault(uint32_t site) {
    record_fault(*device_fault_target(), site);
    asm volatile("exit;" ::: "memory");
    __builtin_unreachable();
}
#endif
SPIRE_FN SPIRE_INLINE constexpr int min(int a, int b) { return a < b ? a : b; }
SPIRE_FN SPIRE_INLINE constexpr int max(int a, int b) { return a > b ? a : b; }
SPIRE_FN SPIRE_INLINE inline int floor_int(float value) {
    const int truncated = static_cast<int>(value);
    return truncated - (value < static_cast<float>(truncated) ? 1 : 0);
}
}
