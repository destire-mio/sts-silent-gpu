#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct InstantKill : ActionHooks {
    SPIRE_WAITING_RULE("InstantKillAction: waiting for a ported caller with three-seed oracle replay")
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
