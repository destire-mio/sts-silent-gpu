#include "training_batch.hpp"
#include "batch.cuh"

namespace spire::cuda {
static int current_device() { int device; check(cudaGetDevice(&device)); return device; }
TrainingBatch::TrainingBatch(int n, uint64_t cap, const std::vector<int64_t>& seeds)
    : size(n), decision_cap(cap), device(current_device()), games(n), actions(n), histories(n),
      tags(n * obs::tag_rows * obs::entity_capacity), candidate_kinds(n * obs::candidate_capacity),
      entity_mask(n * obs::entity_mask_bytes), candidate_mask(n * obs::candidate_mask_bytes),
      ids(n * obs::id_rows * obs::entity_capacity), values(n * obs::value_rows * obs::entity_capacity),
      candidate_refs(n * obs::candidate_rows * obs::candidate_capacity), entity_counts(n), candidate_counts(n),
      finished(n), active(n), done(n), truncated(n), reset_mask(n), advanced(n), rewards(n), faults(n),
      seed_indices(n), decision_counts(n), next_seeds(n), next_seed(1) {
    SPIRE_ASSERT(n > 0 && cap > 0);
    extend_seeds(seeds);
    check(cudaMemset(finished.data, 1, n));
    step(nullptr, nullptr);
}
TrainingBatch::~TrainingBatch() { if (device_seeds_) check(cudaFree(device_seeds_)); }
TrainingState TrainingBatch::state() {
    return {size, decision_cap, games.data, actions.data, histories.data,
        {tags.data, ids.data, values.data, candidate_kinds.data, candidate_refs.data,
         entity_mask.data, candidate_mask.data, entity_counts.data, candidate_counts.data},
        finished.data, active.data, done.data, truncated.data, reset_mask.data, advanced.data,
        rewards.data, faults.data, seed_indices.data, decision_counts.data, next_seeds.data,
        device_seeds_, seeds_.size(), next_seed.data};
}
void TrainingBatch::dispatch(const int32_t* selected, cudaStream_t stream) {
    const auto b = state();
    spire_training_prepare<<<1, 1, 0, stream>>>(b);
    check(cudaGetLastError());
    spire_training_step<<<(size + 31) / 32, 32, 0, stream>>>(b, selected);
    check(cudaGetLastError());
    spire_training_publish<<<(size + 31) / 32, 32, 0, stream>>>(b);
    check(cudaGetLastError());
    spire_training_faults<<<(size + 31) / 32, 32, 0, stream>>>(b);
    check(cudaGetLastError());
    check(cudaStreamSynchronize(stream));
}
void TrainingBatch::step(const int32_t* selected, cudaStream_t stream) {
    check(cudaMemsetAsync(active.data, 1, size, stream));
    dispatch(selected, stream);
}
void TrainingBatch::step_slots(const std::vector<int32_t>& slots, const std::vector<int32_t>& selected) {
    SPIRE_ASSERT(slots.size() == selected.size());
    std::vector<uint8_t> mask(size);
    std::vector<int32_t> choices(size);
    for (std::size_t j = 0; j < slots.size(); ++j) {
        const int i = slots[j];
        SPIRE_ASSERT(i >= 0 && i < size && !mask[i]);
        mask[i] = 1; choices[i] = selected[j];
    }
    active.upload(mask.data());
    Buffer<int32_t> input(size); input.upload(choices.data());
    dispatch(input.data, nullptr);
}
void TrainingBatch::reset_to(int slot, int64_t seed, const std::vector<int32_t>& candidates) {
    reset_to_many({slot}, {seed}, {candidates});
}
void TrainingBatch::reset_to_many(const std::vector<int32_t>& slots, const std::vector<int64_t>& seeds,
                                const std::vector<std::vector<int32_t>>& candidates) {
    SPIRE_ASSERT(!slots.empty() && slots.size() == seeds.size() && slots.size() == candidates.size());
    std::vector<uint8_t> mask(size);
    std::vector<int32_t> offsets{0}, choices;
    for (std::size_t j = 0; j < slots.size(); ++j) {
        const int i = slots[j];
        SPIRE_ASSERT(i >= 0 && i < size && !mask[i]); mask[i] = 1;
        choices.insert(choices.end(), candidates[j].begin(), candidates[j].end());
        offsets.push_back(choices.size());
    }
    active.upload(mask.data());
    Buffer<int32_t> ds(slots.size()), dp(offsets.size()), dc(choices.size());
    Buffer<int64_t> seeds_device(seeds.size());
    ds.upload(slots.data()); dp.upload(offsets.data()); dc.upload(choices.data()); seeds_device.upload(seeds.data());
    spire_training_roots<<<(slots.size() + 31) / 32, 32>>>(state(), ds.data, seeds_device.data, dp.data, dc.data, slots.size());
    check(cudaGetLastError());
    spire_training_faults<<<(size + 31) / 32, 32>>>(state());
    check(cudaGetLastError()); check(cudaDeviceSynchronize());
}
void TrainingBatch::clone(int source, int target) {
    SPIRE_ASSERT(source >= 0 && source < size && target >= 0 && target < size && source != target);
    spire_training_clone<<<1, 1>>>(state(), source, target);
    check(cudaGetLastError()); check(cudaDeviceSynchronize());
}
void TrainingBatch::extend_seeds(const std::vector<int64_t>& seeds) {
    seeds_.insert(seeds_.end(), seeds.begin(), seeds.end());
    if (device_seeds_) { check(cudaFree(device_seeds_)); device_seeds_ = nullptr; }
    if (!seeds_.empty()) {
        check(cudaMalloc(&device_seeds_, seeds_.size() * sizeof(int64_t)));
        check(cudaMemcpy(device_seeds_, seeds_.data(), seeds_.size() * sizeof(int64_t), cudaMemcpyHostToDevice));
    }
}
}
