#pragma once
#include "obs/schema.hpp"

namespace spire::host {
struct LoopHash {
    uint64_t value = 14695981039346656037ULL;
    SPIRE_FN void word(uint64_t x, int bytes) {
        for (int i = 0; i < bytes; ++i) { value = (value ^ (x & 255)) * 1099511628211ULL; x >>= 8; }
    }
    SPIRE_FN void random(const Random& r) {
        word(r.generator.s0, 8); word(r.generator.s1, 8); word(static_cast<uint32_t>(r.counter), 4);
    }
};

SPIRE_FN inline int64_t loop_key(const Game& g, obs::View v) {
    LoopHash h;
    int count = 0;
    for (int i = 0; i < *v.entity_count; ++i)
        if (v.tags[obs::entity_capacity + i] != static_cast<int>(obs::Zone::History)) ++count;
    h.word(count, 4);
    for (int i = 0; i < *v.entity_count; ++i) {
        if (v.tags[obs::entity_capacity + i] == static_cast<int>(obs::Zone::History)) continue;
        for (int r = 0; r < obs::tag_rows; ++r) h.word(v.tags[r * obs::entity_capacity + i], 1);
        for (int r = 0; r < obs::id_rows; ++r) h.word(static_cast<uint16_t>(v.ids[r * obs::entity_capacity + i]), 2);
        for (int r = 0; r < obs::value_rows; ++r) h.word(static_cast<uint16_t>(v.values[r * obs::entity_capacity + i]), 2);
    }
    h.word(*v.candidate_count, 4);
    for (int i = 0; i < *v.candidate_count; ++i) {
        h.word(v.candidate_kinds[i], 1);
        for (int r = 0; r < obs::candidate_rows; ++r) h.word(static_cast<uint16_t>(v.candidate_refs[r * obs::candidate_capacity + i]), 2);
        h.word((v.candidate_mask[i / 8] >> (i % 8)) & 1, 1);
    }
    const auto& r = g.rng;
    h.random(r.monster); h.random(r.map); h.random(r.event); h.random(r.merchant);
    h.random(r.card); h.random(r.treasure); h.random(r.relic); h.random(r.potion);
    h.random(r.monster_hp); h.random(r.ai); h.random(r.shuffle); h.random(r.card_random);
    h.random(r.misc); h.random(r.neow); h.word(r.neow_initialized, 1);
    h.word(g.unseeded.generator.s0, 8); h.word(g.unseeded.generator.s1, 8);
    h.word(static_cast<uint64_t>(g.unseeded.counter), 8);
    return signed_long(h.value);
}
}
