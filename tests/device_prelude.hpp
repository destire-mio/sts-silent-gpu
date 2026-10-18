#pragma once
#define __CUDACC__ 1
#define __device__ __attribute__((device))
#define __host__ __attribute__((host))
#define __global__ __attribute__((global))
#define __shared__ __attribute__((shared))
#include <__clang_cuda_builtin_vars.h>
