#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage_all_enemies.hpp"

namespace spire::units::powers {
struct Panache : PowerHooks {
    SPIRE_STATE(Panache, int damage;)
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) {
        p.state.Panache.damage = p.amount;
        p.amount = 5;
    }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) {
        p.state.Panache.damage += amount;
    }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int, int, Effect&) {
        --p.amount;
        if (p.amount == 0) {
            p.amount = 5;
            int amounts[monster_capacity];
            for (int i = 0; i < g.monster_order.size; ++i) amounts[i] = p.state.Panache.damage;
            add_bottom(g, actions::DamageAllEnemies::make(g, player_target, amounts, g.monster_order.size, DamageType::Thorns));
        }
    }
    static SPIRE_FN void atStartOfTurn(Game&, Power& p, int) { p.amount = 5; }
};
}
