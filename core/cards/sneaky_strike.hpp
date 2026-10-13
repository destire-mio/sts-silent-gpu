#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct SneakyStrike : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 2, 12, -1, -1, 4, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect energy{};
        energy.kind = EffectKind::GainEnergyIfDiscard;
        energy.amount = 2;
        add_bottom(g, energy);
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::SneakyStrike); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
