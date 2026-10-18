#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Nightmare : CardHooks {
    SPIRE_SNAPSHOT("AbstractCard.upgradedCost", scalar, card.upgrades > 0)
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 3, -1, -1, 3, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect nightmare{};
        nightmare.kind = EffectKind::NightmareAction;
        nightmare.source = nightmare.target = player_target;
        nightmare.amount = g.cards[card].magic;
        add_bottom(g, nightmare);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 2);
        }
    }
};
}
