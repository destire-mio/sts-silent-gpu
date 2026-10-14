#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::cards {
struct CutThroughFate : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 7, -1, 2, 2, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::Scry;
        e.source = e.target = no_target;
        e.amount = g.cards[card].magic;
        add_bottom(g, e);
        add_bottom(g, actions::Draw::construct(1, true, -1, player_target));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 2;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
