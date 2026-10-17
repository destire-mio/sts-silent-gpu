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

extern "C" __global__ void spire_encode_batch(spire::Game* games, spire::obs::History* histories,
    spire::obs::BatchView observations, spire::ActionSet* actions, uint32_t* faults, std::size_t count) {
    const std::size_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;
    spire::bind_fault(games[i]);
    faults[i] = spire::obs::encode(games[i], histories[i], observations.view(i), actions[i]).fault;
}

extern "C" __global__ void spire_training_initialize(spire::cuda::TrainingState b) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= b.size) return;
    b.games[i] = spire::initial_game;
    b.histories[i] = {};
    b.finished[i] = 1;
}

extern "C" __global__ void spire_training_prepare(spire::cuda::TrainingState b) {
    for (int i = 0; i < b.size; ++i) {
        b.next_seeds[i] = -1;
        b.rewards[i] = 0; b.advanced[i] = b.reset_mask[i] = 0;
        if (b.active[i] && b.finished[i] && *b.next_seed < b.seed_count) b.next_seeds[i] = (*b.next_seed)++;
    }
}

extern "C" __global__ void spire_training_step(spire::cuda::TrainingState b, const int32_t* selected) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= b.size || !b.active[i]) return;
    auto& g = b.games[i];
    b.done[i] = b.truncated[i] = 0; b.faults[i] = 0;
    if (b.finished[i]) {
        if (b.next_seeds[i] < 0) { b.fail(i, static_cast<uint32_t>(spire::obs::Fault::SeedsExhausted)); return; }
        b.histories[i] = {};
        b.seed_indices[i] = b.next_seeds[i]; b.decision_counts[i] = 0;
        b.reset_mask[i] = 1; b.finished[i] = 0;
        spire::reset(g, b.seeds[b.next_seeds[i]]);
        return;
    }
    if (g.fault) { b.fail(i, g.fault); return; }
    if (b.actions[i].empty()) { b.fail(i, static_cast<uint32_t>(spire::obs::Fault::NoDecision)); return; }
    const int candidate = selected[i];
    if (candidate < 0 || candidate >= b.actions[i].size) {
        b.fail(i, static_cast<uint32_t>(spire::obs::Fault::InvalidCandidate)); return;
    }
    spire::obs::record_action(b.histories[i], g, b.actions[i][candidate]);
    ++b.decision_counts[i]; b.advanced[i] = 1;
    spire::step(g, b.actions[i][candidate]);
}

extern "C" __global__ void spire_training_publish(spire::cuda::TrainingState b) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= b.size || !b.active[i]) return;
    auto& g = b.games[i];
    if (b.faults[i]) return;
    if (g.fault) { b.fail(i, g.fault); return; }
    if (b.advanced[i]) {
        if (g.screen == spire::Screen::Victory || g.screen == spire::Screen::Death) {
            b.finished[i] = 1; b.done[i] = 1;
            b.rewards[i] = g.screen == spire::Screen::Victory ? 1.0f : 0.0f;
        } else if (static_cast<uint64_t>(b.decision_counts[i]) == b.decision_cap) {
            b.finished[i] = 1; b.truncated[i] = 1;
        }
    }
    b.publish(i);
    if (b.advanced[i] && !b.finished[i] && b.actions[i].empty())
        b.fail(i, static_cast<uint32_t>(spire::obs::Fault::NoDecision));
}

extern "C" __global__ void spire_training_faults(spire::cuda::TrainingState b) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < b.size && b.active[i] && !b.faults[i] && b.games[i].fault) b.fail(i, b.games[i].fault);
}

extern "C" __global__ void spire_training_roots(spire::cuda::TrainingState b, const int32_t* slots,
    const int64_t* seeds, const int32_t* offsets, const int32_t* candidates, int count) {
    const int j = blockIdx.x * blockDim.x + threadIdx.x;
    if (j >= count) return;
    const int i = slots[j];
    b.histories[i] = {};
    b.seed_indices[i] = -1; b.decision_counts[i] = 0; b.finished[i] = 0;
    b.rewards[i] = 0; b.done[i] = b.truncated[i] = b.advanced[i] = 0;
    b.reset_mask[i] = 1; b.faults[i] = 0;
    spire::reset(b.games[i], seeds[j]);
    b.publish(i);
    for (int k = offsets[j]; k < offsets[j + 1]; ++k) {
        if (b.faults[i]) return;
        const int candidate = candidates[k];
        if (candidate < 0 || candidate >= b.actions[i].size) {
            b.fail(i, static_cast<uint32_t>(spire::obs::Fault::InvalidCandidate)); return;
        }
        spire::obs::record_action(b.histories[i], b.games[i], b.actions[i][candidate]);
        spire::step(b.games[i], b.actions[i][candidate]);
        b.publish(i);
    }
    if (!b.faults[i] && b.actions[i].empty()) b.fail(i, static_cast<uint32_t>(spire::obs::Fault::NoDecision));
}

extern "C" __global__ void spire_training_clone(spire::cuda::TrainingState b, int source, int target) {
    b.games[target] = b.games[source]; b.histories[target] = b.histories[source];
    b.finished[target] = b.finished[source]; b.done[target] = b.done[source];
    b.truncated[target] = b.truncated[source]; b.faults[target] = b.faults[source];
    b.seed_indices[target] = b.seed_indices[source]; b.decision_counts[target] = b.decision_counts[source];
    b.rewards[target] = 0; b.reset_mask[target] = b.advanced[target] = 0;
    if (b.faults[target]) b.fail(target, b.faults[target]);
    else b.publish(target);
}
