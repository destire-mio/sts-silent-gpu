#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Pummel : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 2, -1, 4, 0, 0, 1, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        for (int i = 1; i < g.cards[card].magic; ++i) {
            Effect e{};
            e.kind = EffectKind::PummelDamage;
            e.source = player_target;
            e.target = target;
            e.amount = e.info_base = g.cards[card].damage;
            add_bottom(g, e);
        }
        queue_card_damage(g, card, target);
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
