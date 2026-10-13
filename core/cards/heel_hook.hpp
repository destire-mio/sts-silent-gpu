#pragma once
#include "../hooks.hpp"
#include "../actions/heel_hook.hpp"

namespace spire::units::cards {
struct HeelHook : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 5, -1, -1, 3, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        DamageInfo info{};
        info.source = player_target;
        info.amount = info.info_base = g.cards[card].damage;
        add_bottom(g, actions::HeelHookAction::construct(target, info));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
