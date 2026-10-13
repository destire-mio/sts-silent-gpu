#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Damage : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true}; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!alive(get_creature(g, e.target)) ||
            (e.damage_type != DamageType::Thorns && e.source != no_target && !alive(get_creature(g, e.source)))) return;
        if (e.extra != 0 && g.run.gold > 0) e.extra = min(e.extra, g.run.gold);
        damage(g, e);
    }
};
}
