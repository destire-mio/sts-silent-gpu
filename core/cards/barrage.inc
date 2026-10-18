#pragma once
#include "core/hooks.hpp"
#include "core/actions/barrage.hpp"
namespace spire::units::cards {
struct Barrage : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 4, -1, -1, 2, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        DamageInfo info{};
        info.amount = info.info_base = g.cards[card].damage;
        add_bottom(g, actions::Barrage::construct(target, info));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
