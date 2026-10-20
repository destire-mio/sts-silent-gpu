#pragma once
#include "core/engine.hpp"

namespace spire::host {
constexpr int progress_columns = 9;
SPIRE_FN inline void training_progress(const Game& g, int64_t* out) {
    out[0] = g.run.floor;
    out[1] = g.run.act;
    out[2] = static_cast<int>(g.room_phase);
    out[3] = static_cast<int>(g.room_kind);
    out[4] = static_cast<int>(g.screen);
    out[5] = g.run.ruby_key | (g.run.emerald_key << 1) | (g.run.sapphire_key << 2);
    out[6] = g.battle_over;
    out[7] = g.smoked;
    out[8] = g.player.creature.hp;
}
}
