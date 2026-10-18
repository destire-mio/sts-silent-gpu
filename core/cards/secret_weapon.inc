#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct SecretWeapon : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 0, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        Effect e{};
        e.kind = EffectKind::AttackFromDeckToHand;
        e.source = e.target = player_target;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.exhaust = false;
        }
    }
    static SPIRE_FN bool canUse(const Game& g, int card, int target) {
        if (!card_can_use_base(g, card, target)) return false;
        for (int i = 0; i < g.draw.size; ++i)
            if (card_data(g.cards[g.draw[i]].id).type == CardType::Attack) return true;
        return false;
    }
};
}
