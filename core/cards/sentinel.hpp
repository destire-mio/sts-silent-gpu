#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"
namespace spire::units::cards {
struct Sentinel : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 5, -1, 0, 3, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
    static SPIRE_FN void triggerOnExhaust(Game& g, int card) {
        add_top(g, actions::GainEnergy::make(g.cards[card].upgrades > 0 ? 3 : 2));
    }
};
}
