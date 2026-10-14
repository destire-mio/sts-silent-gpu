#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct ThunderStrike : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::AllEnemies, 3, 7, -1, 0, 2, 0, 0, false, false};
        d.strike = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        g.cards[card].base_magic = lightning_count(g);
        g.cards[card].magic = g.cards[card].base_magic;
        for (int i = 0; i < g.cards[card].magic; ++i) {
            Effect e{};
            e.kind = EffectKind::NewThunderStrike;
            e.source = e.target = no_target;
            e.card = card;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 2;
        }
    }
    static SPIRE_FN int lightning_count(const Game& g) {
        int count = 0;
        for (int i = 0; i < g.orbs.channeled.size; ++i)
            if (g.orbs.kind(g.orbs.channeled[i]) == OrbKind::Lightning) ++count;
        return count;
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        apply_card_powers(g, card);
        g.cards[card].base_magic = lightning_count(g);
        g.cards[card].magic = 0;
    }
};
}
