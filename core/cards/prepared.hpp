#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Prepared : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 0, -1, -1, 1, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect draw{};
        draw.kind = EffectKind::Draw;
        draw.amount = g.cards[card].magic;
        add_bottom(g, draw);
        Effect discard{};
        discard.kind = EffectKind::Discard;
        discard.amount = g.cards[card].magic;
        add_bottom(g, discard);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
