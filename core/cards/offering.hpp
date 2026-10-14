#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"
#include "../actions/gain_energy.hpp"

namespace spire::units::cards {
struct Offering : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 0, -1, -1, 3, 0, 0, 2, true, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = no_target;
            e.target = no_target;
            add_bottom(g, e);
        }
        Effect e{};
        e.kind = EffectKind::LoseHP;
        e.amount = 6;
        add_bottom(g, e);
        add_bottom(g, units::actions::GainEnergy::make(2));
        add_bottom(g, units::actions::Draw::construct(g.cards[card].magic));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 2;
            card.magic = card.base_magic;
        }
    }
};
}
