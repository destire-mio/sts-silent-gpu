#include "cuda/training_batch.hpp"
#include "value_finediff.hpp"
#include "deck_eval.hpp"
using spire::cuda::TrainingState;

static __global__ void perturb_slots(TrainingState batch, const int* slots, int count, int change, uint8_t* applied) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;
    const int slot = slots[i];
    spire::bind_fault(batch.games[slot]);
    applied[i] = spire::finediff::perturb(batch.games[slot], change);
    if (applied[i]) batch.publish(slot);
}

static __global__ void describe_decks(TrainingState batch, const int* slots, int count,
                                      spire::deck_eval::Features* rows) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < count) rows[i] = spire::deck_eval::describe(batch.games[slots[i]]);
}

static __global__ void edit_decks(TrainingState batch, const int* slots, int count,
                                 const spire::deck_eval::Edit* edits) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;
    const int slot = slots[i];
    spire::bind_fault(batch.games[slot]);
    spire::deck_eval::apply(batch.games[slot], edits[i]);
    batch.publish(slot);
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
    spire::deck_eval::register_features(module);
    module.def("deck_features", [](TrainingBatch& batch, const std::vector<int>& slots) {
        py::list result;
        if (slots.empty()) return result;
        for (int slot : slots) SPIRE_ASSERT(slot >= 0 && slot < batch.size);
        Buffer<int> indices(slots.size());
        Buffer<spire::deck_eval::Features> device_rows(slots.size());
        indices.upload(slots.data());
        describe_decks<<<(slots.size() + 31) / 32, 32>>>(batch.state(), indices.data, slots.size(), device_rows.data);
        check(cudaGetLastError());
        std::vector<spire::deck_eval::Features> rows(slots.size());
        device_rows.download(rows.data());
        for (const auto& row : rows) result.append(spire::deck_eval::describe_python(row));
        return result;
    });
    module.def("edit_decks", [](TrainingBatch& batch, const std::vector<int>& slots,
                                 const std::vector<std::array<int, 4>>& changes) {
        SPIRE_ASSERT(slots.size() == changes.size());
        if (slots.empty()) return;
        for (int slot : slots) SPIRE_ASSERT(slot >= 0 && slot < batch.size);
        std::vector<spire::deck_eval::Edit> edits;
        for (const auto& change : changes) edits.push_back(spire::deck_eval::decode(change));
        Buffer<int> indices(slots.size());
        Buffer<spire::deck_eval::Edit> device_edits(edits.size());
        indices.upload(slots.data());
        device_edits.upload(edits.data());
        edit_decks<<<(slots.size() + 31) / 32, 32>>>(batch.state(), indices.data, slots.size(), device_edits.data);
        check(cudaGetLastError());
        check(cudaDeviceSynchronize());
    });
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
