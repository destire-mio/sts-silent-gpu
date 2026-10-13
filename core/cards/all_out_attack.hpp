#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::cards {
struct AllOutAttack : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemy, 1, 10, -1, -1, 4, 0, 0, false, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::DamageAllEnemies::from_card(g, card));
        Effect e{};
        e.kind = EffectKind::Discard;
        e.source = e.target = player_target;
        e.amount = 1;
        e.extra = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 4;
        }
    }
};
}
