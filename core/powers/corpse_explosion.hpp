#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::powers {
struct CorpseExplosion : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = 1; }
    static SPIRE_FN void onDeath(Game& g, Power& p, int owner) {
        if (!monsters_basically_dead(g) && get_creature(g, owner).hp <= 0) {
            int amounts[monster_capacity];
            for (int i = 0; i < g.monster_order.size; ++i) amounts[i] = get_creature(g, owner).max_hp * p.amount;
            add_bottom(g, actions::DamageAllEnemies::make(g, no_target, amounts, g.monster_order.size, DamageType::Thorns));
        }
    }
};
}
