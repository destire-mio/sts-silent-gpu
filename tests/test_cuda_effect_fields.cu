#include <cuda_runtime.h>
#include "core/state.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

__device__ __attribute__((noinline)) void capture_effect_fields(const Effect& effect, int* output) {
    output[0] = static_cast<int>(effect.power);
    output[1] = static_cast<int>(effect.kind);
    output[2] = effect.source;
    output[3] = effect.target;
    output[4] = effect.amount;
}

__global__ void effect_fields(const Effect* input, int* output) {
    const int owner = threadIdx.x;
    Effect info{};
    info.source = input->source;
    info.target = owner;
    info.info_base = info.amount = 13;
    info.damage_type = input->damage_type;
    capture_effect_fields(info, output + owner * 5);
}

#if !defined(__CUDA_ARCH__)
static void check(cudaError_t error) {
    if (error != cudaSuccess) {
        std::fprintf(stderr, "%s\n", cudaGetErrorString(error));
        std::exit(1);
    }
}

int main() {
    int devices = 0;
    if (cudaGetDeviceCount(&devices) != cudaSuccess || devices == 0) return 77;
    Effect input{};
    Effect* device;
    int* output;
    check(cudaMalloc(&device, sizeof(input)));
    check(cudaMalloc(&output, 32 * 5 * sizeof(int)));
    check(cudaMemcpy(device, &input, sizeof(input), cudaMemcpyHostToDevice));
    effect_fields<<<1, 32>>>(device, output);
    check(cudaDeviceSynchronize());
    int actual[32 * 5];
    check(cudaMemcpy(actual, output, sizeof(actual), cudaMemcpyDeviceToHost));
    for (int owner = 0; owner < 32; ++owner) {
        const int expected[5]{static_cast<int>(PowerId::Strength), static_cast<int>(EffectKind::Draw),
                              player_target, owner, 13};
        for (int field = 0; field < 5; ++field) {
            if (actual[owner * 5 + field] != expected[field]) {
                std::fprintf(stderr, "Effect lane=%d field=%d expected=%d actual=%d\n",
                             owner, field, expected[field], actual[owner * 5 + field]);
                std::exit(1);
            }
        }
    }
    check(cudaFree(device));
    check(cudaFree(output));
    std::puts("CUDA packed Effect fields: 32 lanes PASS");
}
#endif
