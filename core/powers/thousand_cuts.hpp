#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::powers {
struct ThousandCuts : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) { p.amount += amount; }
    static SPIRE_FN void onAfterCardPlayed(Game& g, Power& p, int owner, int) {
            int amounts[monster_capacity];
            for (int i = 0; i < g.monster_order.size; ++i) amounts[i] = p.amount;
            add_bottom(g, actions::DamageAllEnemies::make(owner, amounts, g.monster_order.size, DamageType::Thorns));
    }
};
}
