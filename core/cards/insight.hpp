#pragma once
#include "../hooks.hpp"
#include "../actions/intent_flash.hpp"
#include "../actions/draw.hpp"

namespace spire::units::cards {
struct Insight : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 0, -1, -1, 2, 0, 0, 1, true, false};
        d.self_retain = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::VFX::make());
        add_bottom(g, actions::Draw::construct(g.cards[card].magic, true, -1, player_target));
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
