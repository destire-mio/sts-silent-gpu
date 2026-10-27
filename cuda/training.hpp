#pragma once
#include "obs/encode.hpp"

namespace spire::cuda {
struct TrainingState {
    int size;
    uint64_t decision_cap;
    RunProfile profile;
    Game* games;
    ActionSet* actions;
    obs::History* histories;
    obs::BatchView observations;
    uint8_t *finished, *active, *done, *truncated, *reset_mask, *advanced;
    float* rewards;
    uint32_t* faults;
    int64_t *seed_indices, *decision_counts, *next_seeds;
    const int64_t* seeds;
    uint64_t seed_count;
    uint64_t* next_seed;

    SPIRE_FN void fail(int i, uint32_t fault) const {
        faults[i] = fault; finished[i] = 1;
        done[i] = truncated[i] = 0; rewards[i] = 0;
        observations.view(i).finish(0, 0); actions[i].clear();
    }
    SPIRE_FN void publish(int i) const {
        bind_fault(games[i]);
        const auto result = obs::encode(games[i], histories[i], observations.view(i), actions[i]);
        if (result.fault) fail(i, result.fault);
    }
};
}
