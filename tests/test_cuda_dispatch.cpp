#include "host/training_batch.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#define __global__
struct TestDim { unsigned x = 0; } blockIdx, threadIdx, blockDim{1};
#include "cuda/batch.cu"
#undef __global__

using namespace spire;
static void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct Dispatch {
    host::TrainingBatch buffers;
    std::vector<uint8_t> finished, active;
    std::vector<int64_t> next_seeds, seeds;
    uint64_t next_seed = 0;
    Dispatch(int size, uint64_t cap, const std::vector<int64_t>& input)
        : buffers(size, 1, cap, {}), finished(size, 1), active(size, 1), next_seeds(size), seeds(input) {
        for (threadIdx.x = 0; threadIdx.x < unsigned(size); ++threadIdx.x) spire_training_initialize(state());
        threadIdx.x = 0;
        step(nullptr);
    }
    cuda::TrainingState state() {
        auto& b = buffers;
        return {b.size, b.decision_cap, b.games.data(), b.actions.data(), b.reveals.data(),
            {b.tags.data(), b.ids.data(), b.values.data(), b.candidate_kinds.data(), b.candidate_refs.data(),
             b.entity_mask.data(), b.candidate_mask.data(), b.entity_counts.data(), b.candidate_counts.data()},
            finished.data(), active.data(), b.done.data(), b.truncated.data(), b.reset_mask.data(), b.advanced.data(),
            b.rewards.data(), b.faults.data(), b.seed_indices.data(), b.decision_counts.data(), next_seeds.data(),
            seeds.data(), seeds.size(), &next_seed};
    }
    void step(const int32_t* choices) {
        auto b = state();
        spire_training_prepare(b);
        for (threadIdx.x = 0; threadIdx.x < unsigned(b.size + 3); ++threadIdx.x) spire_training_step(b, choices);
        for (threadIdx.x = 0; threadIdx.x < unsigned(b.size + 3); ++threadIdx.x) spire_training_publish(b);
        for (threadIdx.x = 0; threadIdx.x < unsigned(b.size + 3); ++threadIdx.x) spire_training_faults(b);
        threadIdx.x = 0;
    }
    void compare(const host::TrainingBatch& cpu) {
        auto& b = buffers;
        if (std::memcmp(b.games.data(), cpu.games.data(), sizeof(Game) * b.size)) {
            const auto* a = reinterpret_cast<const unsigned char*>(cpu.games.data());
            const auto* c = reinterpret_cast<const unsigned char*>(b.games.data());
            for (std::size_t i = 0, n = 0; i < sizeof(Game) * b.size && n < 16; ++i) if (a[i] != c[i]) {
                std::fprintf(stderr, "seed=%lld decisions=%lld Game byte %zu: CPU=%u kernel=%u\n",
                    static_cast<long long>(cpu.games[0].run.seed), static_cast<long long>(cpu.decision_counts[0]), i, a[i], c[i]); ++n;
            }
            std::abort();
        }
#define SAME(field) require(b.field == cpu.field, "kernel " #field " differs")
        SAME(tags); SAME(ids); SAME(values); SAME(candidate_kinds); SAME(candidate_refs); SAME(entity_mask);
        SAME(candidate_mask); SAME(entity_counts); SAME(candidate_counts); SAME(done); SAME(truncated);
        SAME(reset_mask); SAME(advanced); SAME(rewards); SAME(faults); SAME(seed_indices); SAME(decision_counts);
#undef SAME
        require(std::memcmp(b.reveals.data(), cpu.reveals.data(), sizeof(obs::History) * b.size) == 0, "kernel history differs");
    }
};
static void routes() {
    std::ifstream input(SPIRE_DEEP_ACTIONS);
    require(input.good(), "missing deep actions");
    int64_t seed; int count, total = 0;
    while (input >> seed >> count) {
        host::TrainingBatch cpu(1, 1, count, {seed, 42});
        Dispatch gpu(1, count, {seed, 42}); gpu.compare(cpu);
        std::vector<int32_t> prefix;
        for (int i = 0; i < count; ++i) {
            int kind, index, target; input >> kind >> index >> target;
            require(input.good(), "incomplete action input");
            int32_t selected = -1;
            for (int j = 0; j < cpu.actions[0].size; ++j) {
                const auto& a = cpu.actions[0][j];
                if (static_cast<int>(a.kind) == kind && a.index == index && a.target == target) selected = j;
            }
            require(selected >= 0, "illegal recorded action"); prefix.push_back(selected);
            cpu.step(&selected); gpu.step(&selected); gpu.compare(cpu); ++total;
        }
        int32_t ignored = -1; cpu.step(&ignored); gpu.step(&ignored); gpu.compare(cpu);
        prefix.resize(prefix.size() - 8);
        cpu.reset_to(0, seed, prefix);
        int32_t slot = 0, offsets[]{0, static_cast<int32_t>(prefix.size())};
        spire_training_roots(gpu.state(), &slot, &seed, offsets, prefix.data(), 1); gpu.compare(cpu);
    }
    require(total == 15038, "deep corpus changed");
    std::puts("CUDA kernel bodies on CPU: 12 routes, 15038 decisions, full Game/observation/status bytes match");
}
static void lifecycle() {
    host::TrainingBatch empty(2, 1, 1, {}); Dispatch no_seeds(2, 1, {}); no_seeds.compare(empty);
    host::TrainingBatch cpu(2, 1, 1, {1, 2}); Dispatch gpu(2, 1, {1, 2});
    int32_t choices[]{-1, 0};
    for (int i = 0; i < 3; ++i) { cpu.step(choices); gpu.step(choices); gpu.compare(cpu); }
    cpu.extend_seeds({3, 4}); gpu.seeds.insert(gpu.seeds.end(), {3, 4});
    cpu.step(choices); gpu.step(choices); gpu.compare(cpu);
    cpu.clone(0, 1); spire_training_clone(gpu.state(), 0, 1); gpu.compare(cpu);
    cpu.step_slots({1}, {0}); gpu.active[0] = 0; choices[1] = 0; gpu.step(choices); gpu.compare(cpu);
    cpu.games[1].fault = gpu.buffers.games[1].fault = 123;
    cpu.step_slots({1}, {0}); gpu.step(choices); gpu.compare(cpu);
    std::puts("CUDA kernel bodies on CPU: cap/reset/invalid candidate/seed exhaustion/clone/subset/latched fault PASS");
}
static void random_steps() {
    std::vector<int64_t> seeds;
    for (int i = 0; i < 4096; ++i) seeds.push_back(1000000000LL + i);
    host::TrainingBatch cpu(33, 1, 97, seeds); Dispatch gpu(33, 97, seeds); gpu.compare(cpu);
    RandomXS128 policy(1729);
    for (int step = 0; step < 256; ++step) {
        std::vector<int32_t> selected(33);
        for (int i = 0; i < 33; ++i) selected[i] = cpu.candidate_counts[i] ? policy.nextInt(cpu.candidate_counts[i]) : -1;
        cpu.step(selected.data()); gpu.step(selected.data()); gpu.compare(cpu);
    }
    std::puts("CUDA kernel bodies on CPU: 8448 random batch slots, full Game/observation/status bytes match");
}
int main() { routes(); lifecycle(); random_steps(); }
