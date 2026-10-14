#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"
#include "../actions/add_card_discard.hpp"

namespace spire::units::cards {
struct Turbo : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 0, -1, -1, 2, 0, 0, 1, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::GainEnergy::make(g.cards[card].magic));
        add_bottom(g, actions::AddCardDiscard::make(create_card(g, make_card(CardId::VoidCard)), 1));
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
