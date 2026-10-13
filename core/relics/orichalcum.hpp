#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Orichalcum : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    SPIRE_STATE(Orichalcum, bool trigger;)
    static SPIRE_FN void onPlayerEndTurn(Game& g, int relic) {
        auto& s = g.run.relics[relic].state.Orichalcum;
        if (g.player.creature.block != 0 && !s.trigger) return;
        s.trigger = false;
        Effect e{};
        e.kind = EffectKind::Block;
        e.amount = 6;
        add_top(g, e);
        queue_relic_above(g, relic, player_target, true);
    }
    static SPIRE_FN void atTurnStart(Game& g, int relic) { g.run.relics[relic].state.Orichalcum.trigger = false; }
    static SPIRE_FN int onPlayerGainedBlock(Game&, int, float amount) { return floor_int(amount); }
    static SPIRE_FN void onVictory(Game&, int) {}
};
}
