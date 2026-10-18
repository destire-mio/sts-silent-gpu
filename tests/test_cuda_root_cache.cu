#if !defined(__CUDA_ARCH__)
#include "cuda/training_batch.hpp"
#include "host/training_batch.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <numeric>

using namespace spire;
namespace device = spire::cuda;

template<class T> static void equal(const device::Buffer<T>& gpu, const std::vector<T>& cpu) {
    std::vector<T> values(cpu.size());
    gpu.download(values.data());
    SPIRE_ASSERT(std::memcmp(values.data(), cpu.data(), cpu.size() * sizeof(T)) == 0);
}

static void compare(device::TrainingBatch& gpu, host::TrainingBatch& cpu) {
#define SAME(field) equal(gpu.field, cpu.field)
    SAME(games); SAME(tags); SAME(ids); SAME(values); SAME(candidate_kinds); SAME(candidate_refs);
    SAME(entity_mask); SAME(candidate_mask); SAME(entity_counts); SAME(candidate_counts);
    SAME(done); SAME(truncated); SAME(reset_mask); SAME(advanced); SAME(rewards); SAME(faults);
    SAME(seed_indices); SAME(decision_counts);
#undef SAME
    equal(gpu.histories, cpu.reveals);
    std::vector<ActionSet> actions(cpu.size);
    gpu.actions.download(actions.data());
    for (int i = 0; i < cpu.size; ++i) {
        SPIRE_ASSERT(actions[i].size == cpu.actions[i].size);
        for (int j = 0; j < actions[i].size; ++j) {
            const auto a = actions[i][j], b = cpu.actions[i][j];
            SPIRE_ASSERT(a.kind == b.kind && a.index == b.index && a.target == b.target);
        }
    }
}

int main(int argc, char** argv) {
    int devices = 0;
    if (cudaGetDeviceCount(&devices) != cudaSuccess || !devices) return 77;
    std::vector<int64_t> seeds;
    std::vector<std::vector<int32_t>> prefixes;
    if (argc == 2) {
        std::ifstream input(argv[1]);
        SPIRE_ASSERT(input.good());
        int64_t seed;
        int count;
        while (input >> seed >> count) {
            std::vector<int32_t> prefix(count);
            for (auto& choice : prefix) input >> choice;
            SPIRE_ASSERT(input.good());
            seeds.push_back(seed); prefixes.push_back(std::move(prefix));
        }
        SPIRE_ASSERT(input.eof() && !seeds.empty());
    } else {
        SPIRE_ASSERT(argc == 1);
        seeds = {123, -1, 1, 2}; prefixes = {{0}, {0}, {}, {999}};
    }
    const int size = 128;
    host::TrainingBatch cpu(size, 1, 3000, {});
    device::TrainingBatch gpu(size, 3000, {});
    host::RootCache host_cache(seeds.size());
    device::RootCache gpu_cache(seeds.size());
    for (std::size_t begin = 0; begin < seeds.size(); begin += size) {
        const int count = std::min<std::size_t>(size, seeds.size() - begin);
        std::vector<int32_t> slots(count);
        std::iota(slots.begin(), slots.end(), 0);
        std::vector<int64_t> selected_seeds(seeds.begin() + begin, seeds.begin() + begin + count);
        std::vector<std::vector<int32_t>> selected_prefixes(prefixes.begin() + begin, prefixes.begin() + begin + count);
        cpu.reset_to_many(slots, selected_seeds, selected_prefixes);
        gpu.reset_to_many(slots, selected_seeds, selected_prefixes);
        compare(gpu, cpu);
        const auto games = cpu.games;
        const auto histories = cpu.reveals;
        const auto faults = cpu.faults;
        std::vector<int64_t> positions(slots.begin(), slots.end()), indices(count);
        std::iota(indices.begin(), indices.end(), begin);
        device::Buffer<int64_t> device_slots(count), device_roots(count);
        device_slots.upload(positions.data()); device_roots.upload(indices.data());
        cpu.cache_roots(host_cache, positions.data(), indices.data(), count);
        gpu.cache_roots(gpu_cache, device_slots.data, device_roots.data, count, nullptr);
        std::vector<Game> cached_games(count);
        std::vector<obs::History> cached_histories(count);
        std::vector<uint32_t> cached_faults(count);
        device::check(cudaMemcpy(cached_games.data(), gpu_cache.games.data + begin, count * sizeof(Game), cudaMemcpyDeviceToHost));
        device::check(cudaMemcpy(cached_histories.data(), gpu_cache.histories.data + begin, count * sizeof(obs::History), cudaMemcpyDeviceToHost));
        device::check(cudaMemcpy(cached_faults.data(), gpu_cache.faults.data + begin, count * sizeof(uint32_t), cudaMemcpyDeviceToHost));
        SPIRE_ASSERT(std::memcmp(cached_games.data(), games.data(), count * sizeof(Game)) == 0);
        SPIRE_ASSERT(std::memcmp(cached_histories.data(), histories.data(), count * sizeof(obs::History)) == 0);
        SPIRE_ASSERT(std::memcmp(cached_faults.data(), faults.data(), count * sizeof(uint32_t)) == 0);
        std::reverse(indices.begin(), indices.end());
        device_roots.upload(indices.data());
        for (int repeat = 0; repeat < 2; ++repeat) {
            cpu.reset_to_many(slots, std::vector<int64_t>(count, 42), std::vector<std::vector<int32_t>>(count));
            gpu.reset_to_many(slots, std::vector<int64_t>(count, 42), std::vector<std::vector<int32_t>>(count));
            cpu.reset_cached(host_cache, positions.data(), indices.data(), count);
            gpu.reset_cached(gpu_cache, device_slots.data, device_roots.data, count, nullptr);
            device::check(cudaDeviceSynchronize());
            compare(gpu, cpu);
            for (int i = 0; i < count; ++i) {
                SPIRE_ASSERT(std::memcmp(&cpu.games[i], &games[count - i - 1], sizeof(Game)) == 0);
                SPIRE_ASSERT(std::memcmp(&cpu.reveals[i], &histories[count - i - 1], sizeof(obs::History)) == 0);
                SPIRE_ASSERT(cpu.faults[i] == faults[count - i - 1]);
            }
        }
        std::printf("CUDA root cache: %zu/%zu prefixes, Game/history/observations/status PASS\n", begin + count, seeds.size());
        std::fflush(stdout);
    }
    std::printf("CUDA root cache: all %zu prefixes match replay bytes; repeated restore leaves cache unchanged\n", seeds.size());
}
#endif
