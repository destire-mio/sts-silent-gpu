#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Expunger : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 1, 9, -1, -1, 6, 0, 0, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        for (int i = 0; i < g.cards[card].magic; ++i) {
            Effect e{};
            e.kind = EffectKind::ExpungeVFX;
            e.source = e.target = no_target;
            e.extra = target;
            add_bottom(g, e);
            queue_card_damage(g, card, target);
        }
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 6;
        }
    }
    static SPIRE_FN Card makeStatEquivalentCopy(Game& g, int card) {
        Card result = stat_equivalent_copy(g, card);
        result.base_magic = g.cards[card].base_magic;
        result.magic = g.cards[card].magic;
        return result;
    }
};
}
