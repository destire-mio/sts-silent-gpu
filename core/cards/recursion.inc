#pragma once
#include "core/hooks.hpp"
#include "core/actions/channel.hpp"
#include "core/actions/animate_orb.hpp"
#include "core/actions/evoke_orb.hpp"
#include "core/actions/evoke_without_removing_orb.hpp"
#include "core/actions/increase_max_orb.hpp"
#include "core/actions/decrease_max_orb.hpp"

namespace spire::units::cards {
struct Recursion : CardHooks {
    static SPIRE_FN constexpr CardDef data() { return {CardType::Skill, CardTarget::Self, 1, -1, -1, -1, 0, 0, 0, false, false}; }
    static SPIRE_FN void use(Game& g, int , int ) {
        Effect e{}; e.kind = EffectKind::Redo; add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 0);
        }
    }
};
}
