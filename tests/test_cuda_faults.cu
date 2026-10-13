#include <cuda_runtime.h>
#include "cuda/batch.cuh"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
static void cuda_check(cudaError_t error) { require(error == cudaSuccess, cudaGetErrorString(error)); }

__global__ void overflow(spire::Game* games, int* reached) {
    const int index = threadIdx.x;
    spire::Game& game = games[index];
    spire::bind_fault(game);
    if (index == 0) {
        game.cards.size = spire::card_capacity;
        game.cards.push_back(spire::Card{});
    } else spire::step(game, {spire::ActionKind::EndTurn});
    reached[index] = 1;
}
__global__ void second_fault(spire::Game* games) {
    spire::bind_fault(games[0]);
    SPIRE_ASSERT(false);
}

int main() {
    int devices = 0;
    if (cudaGetDeviceCount(&devices) != cudaSuccess || devices == 0) return 77;
    spire::Game host[2]{};
    for (int i = 0; i < 2; ++i) {
        spire::reset(host[i], i + 1);
        spire::CombatSetup setup{};
        setup.deck = host[i].run.master_deck;
        spire::start_combat(host[i], setup);
    }
    spire::Game* games;
    int* reached;
    uint32_t* faults;
    spire::Action* actions;
    uint32_t* indices;
    uint64_t* seeds;
    cuda_check(cudaMalloc(&games, sizeof(host)));
    cuda_check(cudaMalloc(&reached, 2 * sizeof(int)));
    cuda_check(cudaMalloc(&faults, 2 * sizeof(uint32_t)));
    cuda_check(cudaMalloc(&actions, 2 * sizeof(spire::Action)));
    cuda_check(cudaMalloc(&indices, sizeof(uint32_t)));
    cuda_check(cudaMalloc(&seeds, sizeof(uint64_t)));
    cuda_check(cudaMemcpy(games, host, sizeof(host), cudaMemcpyHostToDevice));
    cuda_check(cudaMemset(reached, 0, 2 * sizeof(int)));
    overflow<<<1, 2>>>(games, reached);
    cuda_check(cudaDeviceSynchronize());
    int reached_host[2];
    cuda_check(cudaMemcpy(reached_host, reached, sizeof(reached_host), cudaMemcpyDeviceToHost));
    cuda_check(cudaMemcpy(host, games, sizeof(host), cudaMemcpyDeviceToHost));
    require(reached_host[0] == 0 && reached_host[1] == 1 && host[0].fault != 0 && host[1].fault == 0 && host[1].turn == 2,
            "overflow exits only its owning thread before the out-of-bounds store");
    unsigned char saved[sizeof(spire::Game)];
    std::memcpy(saved, &host[0], sizeof(saved));
    const uint32_t first_fault = host[0].fault;
    second_fault<<<1, 1>>>(games);
    spire::Action input[2]{{spire::ActionKind::EndTurn}, {spire::ActionKind::EndTurn}};
    cuda_check(cudaMemcpy(actions, input, sizeof(input), cudaMemcpyHostToDevice));
    spire_step_batch<<<1, 2>>>(games, actions, 2);
    spire_faults_batch<<<1, 2>>>(games, faults, 2);
    cuda_check(cudaDeviceSynchronize());
    uint32_t result[2];
    cuda_check(cudaMemcpy(result, faults, sizeof(result), cudaMemcpyDeviceToHost));
    cuda_check(cudaMemcpy(host, games, sizeof(host), cudaMemcpyDeviceToHost));
    require(result[0] == first_fault && result[1] == 0 && std::memcmp(saved, &host[0], sizeof(saved)) == 0 && host[1].turn == 3,
            "the first site is retained and later batch steps leave the faulted Game unchanged");
    std::memcpy(saved, &host[1], sizeof(saved));
    const uint32_t index = 0;
    const uint64_t seed = 19;
    cuda_check(cudaMemcpy(indices, &index, sizeof(index), cudaMemcpyHostToDevice));
    cuda_check(cudaMemcpy(seeds, &seed, sizeof(seed), cudaMemcpyHostToDevice));
    spire_reset_batch<<<1, 1>>>(games, indices, seeds, 1);
    cuda_check(cudaDeviceSynchronize());
    cuda_check(cudaMemcpy(host, games, sizeof(host), cudaMemcpyDeviceToHost));
    require(host[0].fault == 0 && host[0].run.seed == seed && std::memcmp(saved, &host[1], sizeof(saved)) == 0,
            "indexed device reset preserves the other environment");
    cuda_check(cudaFree(games));
    cuda_check(cudaFree(reached));
    cuda_check(cudaFree(faults));
    cuda_check(cudaFree(actions));
    cuda_check(cudaFree(indices));
    cuda_check(cudaFree(seeds));
}
