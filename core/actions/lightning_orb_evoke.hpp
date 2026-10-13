#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct LightningOrbEvoke : ActionHooks {
    SPIRE_SNAPSHOT("LightningOrbEvokeAction.info", damage_info, effect, queue, index)
    SPIRE_SNAPSHOT("LightningOrbEvokeAction.hitAll", scalar, effect.flag)
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    static SPIRE_FN void execute(Game& g, const Effect& e) { lightning_damage(g, e); }
};
}
