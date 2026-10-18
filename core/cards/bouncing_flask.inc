#pragma once
#include "core/hooks.hpp"
#include "core/actions/intent_flash.hpp"
#include "core/actions/bouncing_flask.hpp"

namespace spire::units::cards {
struct BouncingFlask : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::AllEnemies, 2, -1, -1, 3, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        const int target = random_living_monster(g);
        if (target != no_target) add_bottom(g, actions::VFX::make());
        add_bottom(g, actions::BouncingFlask::construct(target, 3, g.cards[card].magic));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
