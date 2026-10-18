#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct CentennialPuzzle : RelicHooks {
    SPIRE_STATIC_STATE(CentennialPuzzle, bool usedThisCombat;)
    SPIRE_STATIC("usedThisCombat", game.unit_statics.CentennialPuzzle.usedThisCombat)
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atPreBattle(Game& g, int) { g.unit_statics.CentennialPuzzle.usedThisCombat = false; }
    static SPIRE_FN void wasHPLost(Game& g, int relic, int amount) {
        if (amount > 0 && g.room_in_combat && !g.unit_statics.CentennialPuzzle.usedThisCombat) {
            Effect e{};
            e.kind = EffectKind::Draw;
            e.amount = 3;
            add_top(g, e);
            queue_relic_above(g, relic, player_target, true);
            g.unit_statics.CentennialPuzzle.usedThisCombat = true;
            g.run.relics[relic].grayscale = true;
        }
    }
    static SPIRE_FN void justEnteredRoom(Game& g, int relic, int) { g.run.relics[relic].grayscale = false; }
    static SPIRE_FN void onVictory(Game&, int) {}
};
}
