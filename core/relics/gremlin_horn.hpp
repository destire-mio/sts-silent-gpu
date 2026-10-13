#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct GremlinHorn : RelicHooks {
    SPIRE_SNAPSHOT("AbstractRelic.energyBased", scalar, true)
    static SPIRE_FN void onMonsterDeath(Game& g, int relic, int monster) {
        if (g.monsters[monster].creature.hp == 0 && !monsters_basically_dead(g)) {
            queue_relic_above(g, relic, monster, false);
            add_bottom(g, units::actions::GainEnergy::make(1));
            Effect draw{};
            draw.kind = EffectKind::Draw;
            draw.amount = 1;
            add_bottom(g, draw);
        }
    }
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
};
}
