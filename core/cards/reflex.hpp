#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Reflex : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, -2, -1, -1, 2, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect draw{};
        draw.kind = EffectKind::Draw;
        draw.amount = g.cards[card].magic;
        add_bottom(g, draw);
    }
    static SPIRE_FN bool canUse(const Game&, int, int) { return false; }
    static SPIRE_FN void triggerOnManualDiscard(Game& g, int card) {
        Effect draw{};
        draw.kind = EffectKind::Draw;
        draw.amount = g.cards[card].magic;
        add_bottom(g, draw);
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
