#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Brilliance : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 12, -1, 0, 4, 0, 0, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        g.cards[card].damage += g.cards[card].magic;
        calculateCardDamage(g, card, target);
        Effect e{};
        e.kind = EffectKind::Damage;
        e.target = target;
        e.amount = e.info_base = g.cards[card].damage;
        e.flag = true;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 4;
        }
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        Card& c = g.cards[card];
        const int base = c.base_damage;
        c.base_magic = g.manager.mantra_gained;
        c.base_damage += c.base_magic;
        apply_card_powers(g, card);
        c.base_damage = base;
    }
    static SPIRE_FN void calculateCardDamage(Game& g, int card, int target) {
        Card& c = g.cards[card];
        const int base = c.base_damage;
        c.base_magic = g.manager.mantra_gained;
        c.base_damage += c.base_magic;
        calculate_card_damage(g, card, target);
        c.base_damage = base;
    }
};
}
