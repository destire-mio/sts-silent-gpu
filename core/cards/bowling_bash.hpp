#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct BowlingBash : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 7, -1, -1, 3, 0, 0, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        int count = 0;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const Creature& monster = g.monsters[g.monster_order[i]].creature;
            if (monster.dying || monster.half_dead || monster.escaped) continue;
            ++count;
            queue_card_damage(g, card, target);
        }
        if (count >= 3) {
            Effect e{};
            e.kind = EffectKind::SFX;
            e.source = e.target = no_target;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 3;
        }
    }
};
}
