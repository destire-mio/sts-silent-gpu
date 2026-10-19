#pragma once
#include "core/api.hpp"

namespace spire {
struct ActionReferences {
    uint64_t words[(action_capacity + 63) / 64]{};
    SPIRE_FN void add(const Game& g, int index) {
        if (index < 0) return;
        SPIRE_ASSERT(index < action_capacity && g.action_objects.contains(index));
        words[index / 64] |= uint64_t{1} << (index % 64);
    }
};

SPIRE_FN inline ActionReferences action_references(const Game& g) {
    ActionReferences refs{};
    if (g.manager.has_current_action) refs.add(g, g.manager.current_action);
    for (int i = 0; i < g.manager.actions.size; ++i) refs.add(g, g.manager.actions[i]);
    for (int i = 0; i < g.manager.pre_turn_actions.size; ++i) refs.add(g, g.manager.pre_turn_actions[i]);
    uint64_t visited[(action_capacity + 63) / 64]{};
    bool changed = true;
    while (changed) {
        changed = false;
        for (int word = 0; word < (action_capacity + 63) / 64; ++word) {
            uint64_t pending = refs.words[word] & ~visited[word];
            while (pending) {
                const int index = word * 64 + __builtin_ctzll(pending);
                visited[word] |= uint64_t{1} << (index % 64);
                pending &= pending - 1;
                const Effect& e = g.action_objects[index];
                if (e.kind == EffectKind::Draw) refs.add(g, e.payload.state.Draw.follow_up_action - 1);
                if (e.kind == EffectKind::NotStanceCheck) refs.add(g, e.payload.state.NotStanceCheck.action);
                changed = true;
            }
        }
    }
    return refs;
}

SPIRE_FN inline void collect_actions(Game& g) {
    const ActionReferences refs = action_references(g);
    for (int word = 0; word < (action_capacity + 63) / 64; ++word) {
        SPIRE_ASSERT((refs.words[word] & ~g.action_objects.used[word]) == 0);
        uint64_t garbage = g.action_objects.used[word] & ~refs.words[word];
        while (garbage) {
            const int index = word * 64 + __builtin_ctzll(garbage);
            action_release(g, g.action_objects[index]);
            g.action_objects.release(index);
            garbage &= garbage - 1;
        }
    }
}
}
