#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"

namespace spire::units::cards {
struct Bloodletting : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, -1, 2, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::LoseHP;
        e.source = e.target = player_target;
        e.amount = 3;
        add_bottom(g, e);
        add_bottom(g, actions::GainEnergy::make(g.cards[card].magic));
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
