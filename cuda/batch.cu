#include "batch.cuh"

extern "C" __global__ void spire_step_batch(spire::Game* games, const spire::Action* actions,
                                            std::size_t count) {
    const std::size_t index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index < count) spire::step(games[index], actions[index]);
}

extern "C" __global__ void spire_legal_actions_batch(spire::Game* games, spire::ActionSet* actions,
                                                     std::size_t count) {
    const std::size_t index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index < count) {
        spire::bind_fault(games[index]);
        spire::legal_actions(games[index], actions[index]);
    }
}

extern "C" __global__ void spire_faults_batch(const spire::Game* games, uint32_t* faults,
                                              std::size_t count) {
    const std::size_t index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index < count) faults[index] = games[index].fault;
}

extern "C" __global__ void spire_reset_batch(spire::Game* games, const uint32_t* indices,
                                             const uint64_t* seeds, std::size_t count) {
    const std::size_t index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index < count) spire::reset(games[indices[index]], seeds[index]);
}
