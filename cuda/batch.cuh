#pragma once
#include "core/engine.hpp"

extern "C" __global__ void spire_step_batch(spire::Game*, const spire::Action*, std::size_t);
extern "C" __global__ void spire_faults_batch(const spire::Game*, uint32_t*, std::size_t);
extern "C" __global__ void spire_reset_batch(spire::Game*, const uint32_t*, const uint64_t*, std::size_t);
extern "C" __global__ void spire_legal_actions_batch(spire::Game*, spire::ActionSet*, std::size_t);
