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
extern "C" __global__ void spire_training_initialize(spire::cuda::TrainingState);
extern "C" __global__ void spire_training_step(spire::cuda::TrainingState, const int32_t*);
extern "C" __global__ void spire_training_publish(spire::cuda::TrainingState);
extern "C" __global__ void spire_training_faults(spire::cuda::TrainingState);
extern "C" __global__ void spire_training_roots(spire::cuda::TrainingState, const int32_t*, const int64_t*,
                                                const int32_t*, const int32_t*, int);
extern "C" __global__ void spire_training_clone(spire::cuda::TrainingState, int, int);
extern "C" __global__ void spire_training_clone_many(spire::cuda::TrainingState, const int*, const int*, int);
extern "C" __global__ void spire_training_loop_keys(spire::cuda::TrainingState, const int64_t*, int64_t*, int);
extern "C" __global__ void spire_training_progress(spire::cuda::TrainingState, int64_t*);

extern "C" __global__ void spire_training_select(spire::cuda::TrainingState, const uint8_t*);
extern "C" __global__ void spire_training_refill(spire::cuda::TrainingState, const int64_t*, const int64_t*,
    int, const int64_t*, const int64_t*, const int32_t*);
extern "C" __global__ void spire_training_store(spire::cuda::TrainingState, const int64_t*, int,
    uint8_t*, const int64_t*);
extern "C" __global__ void spire_training_cache_roots(spire::cuda::TrainingState, spire::Game*,
    spire::obs::History*, uint32_t*, const int64_t*, const int64_t*, int);
extern "C" __global__ void spire_training_reset_cached(spire::cuda::TrainingState, const spire::Game*,
    const spire::obs::History*, const uint32_t*, const int64_t*, const int64_t*, int);
