#pragma once
#include <cstddef>
#include <cstdlib>
#include "tests/device_prelude.hpp"

struct dim3 {
    unsigned x, y, z;
    constexpr dim3(unsigned x = 1, unsigned y = 1, unsigned z = 1) : x(x), y(y), z(z) {}
};
using cudaStream_t = void*;
using cudaError_t = int;
constexpr cudaError_t cudaSuccess = 0;
enum cudaMemcpyKind { cudaMemcpyHostToDevice, cudaMemcpyDeviceToHost };
cudaError_t cudaGetDevice(int*);
cudaError_t cudaGetDeviceCount(int*);
const char* cudaGetErrorString(cudaError_t);
cudaError_t cudaGetLastError();
cudaError_t cudaDeviceSynchronize();
cudaError_t cudaStreamSynchronize(cudaStream_t);
template<class T> cudaError_t cudaMalloc(T**, std::size_t);
cudaError_t cudaFree(void*);
cudaError_t cudaMemcpy(void*, const void*, std::size_t, cudaMemcpyKind);
cudaError_t cudaMemset(void*, int, std::size_t);
cudaError_t cudaMemsetAsync(void*, int, std::size_t, cudaStream_t);
extern "C" cudaError_t cudaConfigureCall(dim3, dim3, std::size_t = 0, cudaStream_t = nullptr);
extern "C" cudaError_t cudaSetupArgument(const void*, std::size_t, std::size_t);
extern "C" cudaError_t cudaLaunch(const void*);
