#include "core/engine.hpp"

extern "C" __global__ void spire_step_batch(spire::Game* games, const spire::Action* actions,
                                            std::size_t count) {
    const std::size_t index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index < count) spire::step(games[index], actions[index]);
}
