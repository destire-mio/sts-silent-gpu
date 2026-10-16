#include "host/training_batch.hpp"
#include "host/snapshot.hpp"

PYBIND11_MODULE(training_state, module) {
    module.def("prefix_states", [](int64_t seed, const std::vector<int32_t>& prefix) {
        spire::host::TrainingBatch live(1, 1, prefix.size() + 1, {seed});
        spire::host::TrainingBatch replay(1, 1, 1, {});
        for (const auto candidate : prefix) live.step(&candidate);
        replay.reset_to(0, seed, prefix);
        SPIRE_ASSERT(!live.faults[0] && !replay.faults[0]);
        spire::host::FreshRunState fresh{};
        spire::host::initialize_fresh_run(fresh);
        return pybind11::make_tuple(spire::host::SnapshotGraph{live.games[0]}.capture(fresh),
                                   spire::host::SnapshotGraph{replay.games[0]}.capture(fresh));
    });
}
