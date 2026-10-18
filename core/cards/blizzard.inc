#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage_all_enemies.hpp"
namespace spire::units::cards {
struct Blizzard : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 1, 0, -1, 2, 0, 0, 1, false, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        g.cards[card].base_damage = frost_count(g) * g.cards[card].magic;
        card_calculateCardDamage(g, card, no_target);
        add_bottom(g, actions::DamageAllEnemies::from_card(g, card));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
    static SPIRE_FN int frost_count(const Game& g) {
        int count = 0;
        for (int i = 0; i < g.orbs.channeled.size; ++i)
            if (g.orbs.kind(g.orbs.channeled[i]) == OrbKind::Frost)
                ++count;
        return count;
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        const int count = frost_count(g);
        if (count > 0) {
            get_card(g, card).base_damage = count * get_card(g, card).magic;
            apply_card_powers(g, card);
        }
    }
};
}
