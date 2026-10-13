#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"

namespace spire::units::cards {
struct Tactician : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, -2, -1, -1, 1, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game&, int, int) {

    }
    static SPIRE_FN bool canUse(const Game&, int, int) { return false; }
    static SPIRE_FN void triggerOnManualDiscard(Game& g, int card) {
        add_top(g, actions::GainEnergy::make(g.cards[card].magic));
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::Tactician); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
