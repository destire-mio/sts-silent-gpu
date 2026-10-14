#pragma once
#include "../hooks.hpp"
#include "../actions/change_stance.hpp"

namespace spire::units::cards {
struct Tantrum : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 1, 3, -1, 3, 0, 0, 1, false, false};
        d.shuffle_back_into_draw_pile = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        for (int i = 0; i < g.cards[card].magic; ++i) queue_card_damage(g, card, target);
        add_bottom(g, actions::ChangeStance::make(Stance::Wrath));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
