#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct LightningOrbPassive : ActionHooks {
    SPIRE_SNAPSHOT("LightningOrbPassiveAction.info", damage_info, effect, queue, index)
    SPIRE_SNAPSHOT("LightningOrbPassiveAction.hitAll", scalar, effect.flag)
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("LightningOrbPassiveAction.orb", orb, effect.extra)
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    static SPIRE_FN void execute(Game& g, const Effect& e) { lightning_damage(g, e); }
};
}
