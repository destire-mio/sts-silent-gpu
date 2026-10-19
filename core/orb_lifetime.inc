#pragma once
#include "core/api.hpp"

namespace spire {
struct OrbReferences {
    uint64_t words[2]{};
    SPIRE_FN void add(const Game& g, int index) {
        if (index < 0) return;
        SPIRE_ASSERT(index < 128 && g.orbs.objects.contains(index));
        words[index / 64] |= uint64_t{1} << (index % 64);
    }
};

SPIRE_FN inline OrbReferences orb_references(const Game& g) {
    OrbReferences refs{};
    for (int i = 0; i < g.orbs.slots.size; ++i) refs.add(g, g.orbs.slots[i]);
    for (int i = 0; i < g.orbs.channeled.size; ++i) refs.add(g, g.orbs.channeled[i]);
    for (int word = 0; word < (action_capacity + 63) / 64; ++word) {
        uint64_t used = g.action_objects.used[word];
        while (used) {
            const Effect& e = g.action_objects[word * 64 + __builtin_ctzll(used)];
            if (e.kind == EffectKind::Channel) refs.add(g, e.extra);
            used &= used - 1;
        }
    }
    return refs;
}

SPIRE_FN inline void collect_orbs(Game& g) {
    // AbstractPlayer.channelOrb discards the constructed orb when maxOrbs is zero.
    const OrbReferences refs = orb_references(g);
    for (int word = 0; word < 2; ++word) {
        SPIRE_ASSERT((refs.words[word] & ~g.orbs.objects.used[word]) == 0);
        uint64_t garbage = g.orbs.objects.used[word] & ~refs.words[word];
        while (garbage) {
            g.orbs.objects.release(word * 64 + __builtin_ctzll(garbage));
            garbage &= garbage - 1;
        }
    }
}
}
