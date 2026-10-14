#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Ragnarok : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 3, 5, -1, 5, 1, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        for (int i = 0; i < g.cards[card].magic; ++i) {
            Effect e{};
            e.kind = EffectKind::AttackDamageRandomEnemy;
            e.source = e.target = no_target;
            e.card = card;
            e.flag = true;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 1;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
