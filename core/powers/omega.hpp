#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::powers {
struct Omega : PowerHooks {
    SPIRE_WAITING_RULE("OmegaPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int, bool is_player) {
        if (!is_player)
            return;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const Creature &c = g.monsters[g.monster_order[i]].creature;
            if (!c.dead && !c.escaped) {
                Effect e{};
                e.kind = EffectKind::VFX;
                e.source = e.target = no_target;
                add_bottom(g, e);
            }
        }
        int amounts[monster_capacity]{};
        for (int i = 0; i < g.monster_order.size; ++i)
            amounts[i] = p.amount;
        add_bottom(
            g, units::actions::DamageAllEnemies::make(g, no_target, amounts, g.monster_order.size, DamageType::Thorns));
    }
};
}
