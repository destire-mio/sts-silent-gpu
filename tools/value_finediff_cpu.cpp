#include "host/training_batch.hpp"
#include "value_finediff.hpp"
#include "deck_eval.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using spire::host::TrainingBatch;

PYBIND11_MODULE(spire_finediff, module) {
    py::module_::import("spire");
    spire::deck_eval::register_features(module);
    module.def("deck_features", [](const TrainingBatch& batch, const std::vector<int>& slots) {
        py::list rows;
        for (int slot : slots) {
            SPIRE_ASSERT(slot >= 0 && slot < batch.size);
            rows.append(spire::deck_eval::describe_python(spire::deck_eval::describe(batch.games[slot])));
        }
        return rows;
    });
    module.def("edit_decks", [](TrainingBatch& batch, const std::vector<int>& slots,
                                 const std::vector<std::array<int, 4>>& edits) {
        SPIRE_ASSERT(slots.size() == edits.size());
        for (size_t i = 0; i < slots.size(); ++i) {
            const int slot = slots[i];
            SPIRE_ASSERT(slot >= 0 && slot < batch.size && !batch.faults[slot]);
            spire::deck_eval::apply(batch.games[slot], spire::deck_eval::decode(edits[i]));
            batch.publish(slot);
        }
    });
    module.def("perturb_slots", [](TrainingBatch& batch, const std::vector<int>& slots, int change) {
        std::vector<bool> applied;
        for (int slot : slots) {
            SPIRE_ASSERT(slot >= 0 && slot < batch.size && !batch.faults[slot]);
            const bool changed = spire::finediff::perturb(batch.games[slot], change);
            applied.push_back(changed);
            if (changed) batch.publish(slot);
        }
        return applied;
    });
    module.def("equal_except_hp", [](const TrainingBatch& batch, int baseline, int variant, int delta) {
        SPIRE_ASSERT(baseline >= 0 && baseline < batch.size && variant >= 0 && variant < batch.size);
        const auto& source = batch.games[baseline];
        const auto& changed = batch.games[variant];
        if (source.player.creature.hp - changed.player.creature.hp != delta) return false;
        const auto* a = reinterpret_cast<const char*>(&source);
        const auto* b = reinterpret_cast<const char*>(&changed);
        const auto hp = reinterpret_cast<const char*>(&source.player.creature.hp) - a;
        return std::char_traits<char>::compare(a, b, hp) == 0
            && std::char_traits<char>::compare(a + hp + sizeof(int), b + hp + sizeof(int),
                                              sizeof(source) - hp - sizeof(int)) == 0
            && std::char_traits<char>::compare(reinterpret_cast<const char*>(&batch.reveals[baseline]),
                    reinterpret_cast<const char*>(&batch.reveals[variant]), sizeof(spire::obs::History)) == 0;
    });
}
