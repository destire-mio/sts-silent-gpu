#pragma once
#include "core/hooks.hpp"
#include "core/actions/intent_flash.hpp"
#include "core/actions/draw.hpp"
#include "core/actions/gain_energy.hpp"

namespace spire::units::cards {
struct Adrenaline : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::VFX::make());
        add_bottom(g, actions::GainEnergy::make(g.cards[card].upgrades > 0 ? 2 : 1));
        add_bottom(g, actions::Draw::construct(2, true, -1, player_target));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
