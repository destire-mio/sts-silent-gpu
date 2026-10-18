#pragma once
#include "core/hooks.hpp"
#include "core/actions/draw.hpp"

namespace spire::units::cards {
struct BattleTrance : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 0, -1, -1, 3, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::Draw::construct(g.cards[card].magic, true, -1, player_target));
        queue_power(g, player_target, player_target, PowerId::NoDraw, -1);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
