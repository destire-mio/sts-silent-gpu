#if !defined(__CUDA_ARCH__)
#include "cuda/training_batch.hpp"
#include "cuda/batch.cuh"
#include "host/training_batch.hpp"
#include <cstring>
#include <fstream>

using namespace spire;
namespace device = spire::cuda;
static void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
template<class T> static void equal(const device::Buffer<T>& device, const std::vector<T>& cpu, const char* name) {
    std::vector<T> got(cpu.size()); device.download(got.data());
    if (std::memcmp(got.data(), cpu.data(), cpu.size() * sizeof(T))) {
        const auto* a = reinterpret_cast<const unsigned char*>(cpu.data());
        const auto* b = reinterpret_cast<const unsigned char*>(got.data());
        for (std::size_t i = 0; i < cpu.size() * sizeof(T); ++i) if (a[i] != b[i]) {
            std::fprintf(stderr, "%s byte %zu: CPU=%u GPU=%u\n", name, i, a[i], b[i]); break;
        }
        std::abort();
    }
}
static void compare(device::TrainingBatch& gpu, host::TrainingBatch& cpu) {
#define SAME(field) equal(gpu.field, cpu.field, #field)
    SAME(games); SAME(tags); SAME(ids); SAME(values); SAME(candidate_kinds); SAME(candidate_refs);
    SAME(entity_mask); SAME(candidate_mask); SAME(entity_counts); SAME(candidate_counts);
    SAME(done); SAME(truncated); SAME(reset_mask); SAME(advanced); SAME(rewards); SAME(faults);
    SAME(seed_indices); SAME(decision_counts);
#undef SAME
    equal(gpu.histories, cpu.reveals, "histories");
    std::vector<ActionSet> actions(cpu.size); gpu.actions.download(actions.data());
    for (int i = 0; i < cpu.size; ++i) {
        require(actions[i].size == cpu.actions[i].size, "candidate count differs");
        for (int j = 0; j < actions[i].size; ++j) {
            const auto& a = actions[i][j]; const auto& b = cpu.actions[i][j];
            require(a.kind == b.kind && a.index == b.index && a.target == b.target, "candidate action differs");
        }
    }
}
static void advance(device::TrainingBatch& gpu, host::TrainingBatch& cpu, const std::vector<int32_t>& selected) {
    device::Buffer<int32_t> device(selected.size()); device.upload(selected.data());
    gpu.step(device.data, nullptr); cpu.step(selected.data()); compare(gpu, cpu);
}
static void routes() {
    std::ifstream input(SPIRE_DEEP_ACTIONS);
    require(input.good(), "missing deep action inputs");
    int64_t seed; int count, total = 0, games = 0;
    while (input >> seed >> count) {
        host::TrainingBatch cpu(1, 1, count, {seed, 42});
        device::TrainingBatch gpu(1, count, {seed, 42});
        auto state = gpu.state();
        spire_encode_batch<<<1, 32>>>(state.games, state.histories, state.observations, state.actions, state.faults, 1);
        device::check(cudaGetLastError()); device::check(cudaDeviceSynchronize());
        compare(gpu, cpu);
        std::vector<int32_t> prefix;
        for (int i = 0; i < count; ++i) {
            int kind, index, target; input >> kind >> index >> target;
            require(input.good(), "incomplete deep action input");
            int selected = -1;
            for (int j = 0; j < cpu.actions[0].size; ++j) {
                const auto& a = cpu.actions[0][j];
                if (static_cast<int>(a.kind) == kind && a.index == index && a.target == target) selected = j;
            }
            require(selected >= 0, "recorded action is not legal");
            prefix.push_back(selected);
            advance(gpu, cpu, {selected}); ++total;
            require(!cpu.faults[0] && !cpu.truncated[0], "deep route fault or premature cap");
        }
        require(cpu.done[0], "missing deep terminal");
        advance(gpu, cpu, {-1});
        prefix.resize(prefix.size() - 8);
        cpu.reset_to(0, seed, prefix); gpu.reset_to(0, seed, prefix); compare(gpu, cpu);
        std::printf("CUDA full Game/observation bytes: seed=%lld decisions=%d PASS\n", static_cast<long long>(seed), count);
        ++games;
    }
    require(games == 12 && total == 15038, "deep corpus changed");
}
static void random_and_lifecycle() {
    host::TrainingBatch empty(2, 1, 1, {});
    device::TrainingBatch no_seeds(2, 1, {}); compare(no_seeds, empty);
    std::vector<int64_t> seeds;
    for (int i = 0; i < 4096; ++i) seeds.push_back(1000000000LL + i);
    host::TrainingBatch cpu(33, 1, 97, seeds);
    device::TrainingBatch gpu(33, 97, seeds);
    compare(gpu, cpu);
    RandomXS128 policy(1729);
    for (int step = 0; step < 256; ++step) {
        std::vector<int32_t> selected(33);
        for (int i = 0; i < 33; ++i) selected[i] = cpu.candidate_counts[i] ? policy.nextInt(cpu.candidate_counts[i]) : -1;
        advance(gpu, cpu, selected);
    }
    cpu.clone(0, 32); gpu.clone(0, 32); compare(gpu, cpu);
    cpu.step_slots({0, 32}, {-1, 0}); gpu.step_slots({0, 32}, {-1, 0}); compare(gpu, cpu);
    cpu.reset_to_many({32, 0}, {123, -1}, {{0}, {0}});
    gpu.reset_to_many({32, 0}, {123, -1}, {{0}, {0}}); compare(gpu, cpu);
    host::TrainingBatch capped(2, 1, 1, {1, 2});
    device::TrainingBatch device_capped(2, 1, {1, 2});
    advance(device_capped, capped, {0, 0}); advance(device_capped, capped, {-1, -1});
    capped.extend_seeds({3, 4}); device_capped.extend_seeds({3, 4});
    advance(device_capped, capped, {-1, -1});
    std::puts("CUDA random/lifecycle: 8448 batch slots, cap/reset/fault isolation/clone/root replay/seed exhaustion PASS");
}
static void determinized_slots() {
    host::TrainingBatch cpu(3, 1, 1000, {1, 2, 3});
    device::TrainingBatch gpu(3, 1000, {1, 2, 3});
    for (int slot = 0; slot < 2; ++slot) {
        CombatSetup setup; setup.encounter = EncounterId::JawWorm;
        setup.deck = cpu.games[slot].run.master_deck;
        for (int i = 0; i < cpu.games[slot].run.master_deck.size; ++i)
            setup.deck.push_back(cpu.games[slot].run.master_deck[i]);
        if (slot == 1) setup.relics.push_back({RelicId::FrozenEye});
        start_combat(cpu.games[slot], setup);
        step(cpu.games[slot], {ActionKind::EndTurn}); cpu.publish(slot);
    }
    gpu.games.upload(cpu.games.data()); gpu.histories.upload(cpu.reveals.data());
    auto state = gpu.state();
    spire_encode_batch<<<1, 32>>>(state.games, state.histories, state.observations, state.actions, state.faults, 3);
    device::check(cudaGetLastError()); device::check(cudaDeviceSynchronize());
    compare(gpu, cpu);
    const Game root = cpu.games[0];
    for (uint64_t seed : {0ULL, 42ULL, 0xffffffffffffffffULL}) {
        cpu.determinize_slots({2, 1, 0}, {seed, seed ^ 1, seed ^ 2});
        gpu.determinize_slots({2, 1, 0}, {seed, seed ^ 1, seed ^ 2}); compare(gpu, cpu);
    }
    require(std::memcmp(&root.draw, &cpu.games[0].draw, sizeof(Pile)) != 0, "parity fixture did not shuffle");
    std::vector<int32_t> selected(3);
    for (int i = 0; i < 2; ++i)
        while (cpu.actions[i][selected[i]].kind != ActionKind::EndTurn) ++selected[i];
    advance(gpu, cpu, selected);
    cpu.determinize_slots({}, {}); gpu.determinize_slots({}, {}); compare(gpu, cpu);
    std::puts("CUDA determinize: full Game/observations/actions/status and continuation match CPU");
}
int main() {
    int devices; device::check(cudaGetDeviceCount(&devices)); require(devices > 0, "CUDA parity requires a GPU");
    routes(); random_and_lifecycle(); determinized_slots();
    std::puts("CUDA parity: full Game bytes and all observation/status bytes match after every step");
}
#endif
