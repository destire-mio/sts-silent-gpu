#pragma once
#include "core/hooks.hpp"
#include "core/actions/channel.hpp"
namespace spire::units::cards {
struct Chill : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, -1, 1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        int count = 0;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const Creature& monster = g.monsters[g.monster_order[i]].creature;
            if (!monster.dying && !monster.half_dead && !monster.escaped)
                ++count;
        }
        for (int i = 0; i < count * g.cards[card].magic; ++i)
            add_bottom(g, actions::Channel::make(make_orb(g, OrbKind::Frost)));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.innate = true;
        }
    }
};
}
