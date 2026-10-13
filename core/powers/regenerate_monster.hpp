#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct RegenerateMonster : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int owner, bool) {
        const Creature& c = get_creature(g, owner);
        if (!c.half_dead && !c.dying && !c.dead) {
            Effect e{};
            e.kind = EffectKind::Heal;
            e.source = e.target = owner;
            e.amount = p.amount;
            add_bottom(g, e);
        }
    }
};
}
