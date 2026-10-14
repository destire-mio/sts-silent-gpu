#pragma once
#include "../hooks.hpp"
#include "../actions/intent_flash.hpp"
#include "../actions/gain_energy.hpp"

namespace spire::units::cards {
struct Miracle : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::None, 0, -1, -1, -1, 0, 0, 0, true, false};
        d.self_retain = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::VFX::make());
        add_bottom(g, actions::VFX::make());
        add_bottom(g, actions::GainEnergy::make(g.cards[card].upgrades > 0 ? 2 : 1));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
