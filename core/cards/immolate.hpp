#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"
#include "../actions/add_card_discard.hpp"

namespace spire::units::cards {
struct Immolate : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::AllEnemies, 2, 21, -1, -1, 7, 0, 0, false, false};
        d.multi_damage = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::DamageAllEnemies::from_card(g, card));
        add_bottom(g, actions::AddCardDiscard::make(create_card(g, make_card(CardId::Burn)), 1));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 7;
        }
    }
};
}
