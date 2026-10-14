#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Clash : CardHooks {
    static SPIRE_FN bool canUse(const Game& g, int card, int target) {
        if (!card_can_use_base(g, card, target)) return false;
        for (int i = 0; i < g.hand.size; ++i)
            if (card_data(g.cards[g.hand[i]].id).type != CardType::Attack) return false;
        return true;
    }
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 0, 14, -1, -1, 4, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (target != no_target) {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = e.target = no_target;
            add_bottom(g, e);
        }
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_card_damage(card, data().upgrade_damage);
        }
    }
};
}
