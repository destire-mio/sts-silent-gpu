#include "cuda/training_batch.hpp"
#include "value_finediff.hpp"
using spire::cuda::TrainingState;

static __global__ void perturb_slots(TrainingState batch, const int* slots, int count, int change, uint8_t* applied) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;
    const int slot = slots[i];
    spire::bind_fault(batch.games[slot]);
    applied[i] = spire::finediff::perturb(batch.games[slot], change);
    if (applied[i]) batch.publish(slot);
}

#if !defined(__CUDA_ARCH__)
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using spire::cuda::Buffer;
using spire::cuda::TrainingBatch;
using spire::cuda::check;

PYBIND11_MODULE(spire_gpu_finediff, module) {
    py::module_::import("spire_gpu");
    module.def("perturb_slots", [](TrainingBatch& batch, const std::vector<int>& slots, int change) {
        std::vector<uint8_t> applied(slots.size());
        if (slots.empty()) return applied;
        for (int slot : slots) SPIRE_ASSERT(slot >= 0 && slot < batch.size);
        Buffer<int> indices(slots.size());
        Buffer<uint8_t> results(slots.size());
        indices.upload(slots.data());
        perturb_slots<<<(slots.size() + 31) / 32, 32>>>(batch.state(), indices.data, slots.size(), change, results.data);
        check(cudaGetLastError());
        check(cudaDeviceSynchronize());
        results.download(applied.data());
        return applied;
    });
}
#endif
