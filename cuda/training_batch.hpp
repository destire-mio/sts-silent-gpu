#pragma once
#include "training.hpp"
#include <cuda_runtime.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace spire::cuda {
inline void check(cudaError_t error) {
    if (error != cudaSuccess) { std::fprintf(stderr, "CUDA: %s\n", cudaGetErrorString(error)); std::abort(); }
}
template<class T> struct Buffer {
    T* data = nullptr;
    std::size_t size;
    explicit Buffer(std::size_t count) : size(count) {
        if (size) { check(cudaMalloc(&data, size * sizeof(T))); check(cudaMemset(data, 0, size * sizeof(T))); }
    }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    ~Buffer() { if (data) check(cudaFree(data)); }
    void upload(const T* source) { if (size) check(cudaMemcpy(data, source, size * sizeof(T), cudaMemcpyHostToDevice)); }
    void download(T* target) const { if (size) check(cudaMemcpy(target, data, size * sizeof(T), cudaMemcpyDeviceToHost)); }
};

struct TrainingBatch {
    const int size;
    const uint64_t decision_cap;
    const int device;
    Buffer<Game> games;
    Buffer<ActionSet> actions;
    Buffer<obs::History> histories;
    Buffer<uint8_t> tags, candidate_kinds, entity_mask, candidate_mask;
    Buffer<int16_t> ids, values, candidate_refs;
    Buffer<int32_t> entity_counts, candidate_counts;
    Buffer<uint8_t> finished, active, done, truncated, reset_mask, advanced;
    Buffer<float> rewards;
    Buffer<uint32_t> faults;
    Buffer<int64_t> seed_indices, decision_counts, next_seeds;
    Buffer<uint64_t> next_seed;

    TrainingBatch(int size, uint64_t decision_cap, const std::vector<int64_t>& seeds);
    TrainingBatch(const TrainingBatch&) = delete;
    TrainingBatch& operator=(const TrainingBatch&) = delete;
    ~TrainingBatch();
    TrainingState state();
    void step(const int32_t* selected, cudaStream_t stream);
    void step_slots(const std::vector<int32_t>& slots, const std::vector<int32_t>& selected);
    void reset_to(int slot, int64_t seed, const std::vector<int32_t>& candidates);
    void reset_to_many(const std::vector<int32_t>& slots, const std::vector<int64_t>& seeds,
                       const std::vector<std::vector<int32_t>>& candidates);
    void clone(int source, int target);
    void extend_seeds(const std::vector<int64_t>& seeds);
private:
    std::vector<int64_t> seeds_;
    int64_t* device_seeds_ = nullptr;
    void dispatch(const int32_t* selected, cudaStream_t stream);
};
}
