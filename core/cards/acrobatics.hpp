#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::cards {
struct Acrobatics : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, 3, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::Draw::construct(g.cards[card].magic, true, -1, player_target));
        Effect e{};
        e.kind = EffectKind::Discard;
        e.source = e.target = player_target;
        e.amount = 1;
        add_bottom(g, e);
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
