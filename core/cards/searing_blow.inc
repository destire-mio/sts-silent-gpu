#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct SearingBlow : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 2, 12, -1, -1, 4, 0, 0, false, false};
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
        card.base_damage += 4 + card.upgrades;
        SPIRE_ASSERT(card.upgrades < 65535);
        ++card.upgrades;
        card.state.SearingBlow.flags = 3;
    }
    SPIRE_STATE(SearingBlow, uint8_t flags;)
    SPIRE_SNAPSHOT("AbstractCard.upgraded", scalar, static_cast<bool>(card.state.SearingBlow.flags & 1))
    SPIRE_SNAPSHOT("AbstractCard.upgradedDamage", scalar, static_cast<bool>(card.state.SearingBlow.flags & 2))
    static SPIRE_FN bool canUpgrade(const Game&, int) { return true; }
    static SPIRE_FN Card makeCopy(const Game& g, int card) {
        // SearingBlow.makeCopy preserves the count without applying its upgrades.
        Card result = make_card(CardId::SearingBlow);
        result.upgrades = get_card(g, card).upgrades;
        return result;
    }
    static SPIRE_FN Card makeStatEquivalentCopy(Game& g, int card) {
        const Card& source = get_card(g, card);
        Card result = stat_equivalent_copy(g, card);
        // AbstractCard.makeStatEquivalentCopy restores upgraded but keeps upgradeDamage's flag.
        result.state.SearingBlow.flags = (source.state.SearingBlow.flags & 1) | (source.upgrades > 0 ? 2 : 0);
        return result;
    }
};
}
