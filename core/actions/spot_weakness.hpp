#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct SpotWeakness : ActionHooks {
    SPIRE_WAITING_RULE("SpotWeaknessAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_SNAPSHOT("SpotWeaknessAction.targetMonster", creature, effect.target)
    SPIRE_SNAPSHOT("SpotWeaknessAction.damageIncrease", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (g.monsters[e.target].intent_base_damage >= 0)
            queue_power(g, player_target, player_target, PowerId::Strength, e.extra);
    }
};
}
