#pragma once
#include "core/engine.hpp"

extern "C" __global__ void spire_step_batch(spire::Game*, const spire::Action*, std::size_t);
extern "C" __global__ void spire_faults_batch(const spire::Game*, uint32_t*, std::size_t);
extern "C" __global__ void spire_reset_batch(spire::Game*, const uint32_t*, const uint64_t*, std::size_t);
extern "C" __global__ void spire_legal_actions_batch(spire::Game*, spire::ActionSet*, std::size_t);

#include "training.hpp"
extern "C" __global__ void spire_encode_batch(spire::Game*, spire::obs::History*, spire::obs::BatchView,
                                              spire::ActionSet*, uint32_t*, std::size_t);
extern "C" __global__ void spire_training_prepare(spire::cuda::TrainingState);
extern "C" __global__ void spire_training_step(spire::cuda::TrainingState, const int32_t*);
extern "C" __global__ void spire_training_publish(spire::cuda::TrainingState);
extern "C" __global__ void spire_training_faults(spire::cuda::TrainingState);
extern "C" __global__ void spire_training_roots(spire::cuda::TrainingState, const int32_t*, const int64_t*,
                                                const int32_t*, const int32_t*, int);
extern "C" __global__ void spire_training_clone(spire::cuda::TrainingState, int, int);
