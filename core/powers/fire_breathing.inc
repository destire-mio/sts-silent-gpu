#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage_all_enemies.hpp"

namespace spire::units::powers {
struct FireBreathing : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onCardDraw(Game& g, Power& p, int, int card) {
        const CardType type = card_data(g.cards[card].id).type;
        if (type == CardType::Status || type == CardType::Curse) {
            int amounts[monster_capacity]{};
            for (int i = 0; i < g.monster_order.size; ++i)
                amounts[i] = p.amount;
            add_bottom(g, units::actions::DamageAllEnemies::make(g, no_target, amounts, g.monster_order.size,
                                                                 DamageType::Thorns));
        }
    }
};
}
