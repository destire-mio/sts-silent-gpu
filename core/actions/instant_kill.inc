#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct InstantKill : ActionHooks {
    static SPIRE_FN void execute(Game& g, Effect& e) {
        Creature &c = get_creature(g, e.target);
        c.hp = 0;
        Effect hit{};
        hit.source = no_target;
        hit.target = e.target;
        hit.damage_type = DamageType::HpLoss;
        creature_damage(g, hit);
    }
};
}
