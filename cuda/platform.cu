#include "core/platform.hpp"
#if defined(__CUDA_ARCH__)
namespace spire {
SPIRE_FN uint32_t*& device_fault_target() {
    __shared__ uint32_t* targets[1024];
    const unsigned thread = threadIdx.x + blockDim.x * (threadIdx.y + blockDim.y * threadIdx.z);
    return targets[thread];
}
}
#endif
