#pragma once
#include "core/hooks.hpp"
#include "core/actions/change_stance.hpp"

namespace spire::units::cards {
struct Blasphemy : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 1, -1, -1, -1, 0, 0, 0, true, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int, int) {
        add_bottom(g, units::actions::ChangeStance::make(Stance::Divinity));
        queue_power(g, player_target, player_target, PowerId::EndTurnDeath, -1, false, 0);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.self_retain = true;
        }
    }
};
}
