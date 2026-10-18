#pragma once
#include "core/hooks.hpp"
#include "core/actions/gain_energy.hpp"

namespace spire::units::actions {
struct FollowUp : ActionHooks {
    static SPIRE_FN void execute(Game& g, Effect &) {
        if (g.manager.played_cards.size >= 2 &&
            card_data(g.cards[g.manager.played_cards.values[g.manager.played_cards.size - 2]].id).type ==
                CardType::Attack) {
            add_top(g, units::actions::GainEnergy::make(1));
            Effect v{};
            v.kind = EffectKind::VFX;
            v.source = v.target = no_target;
            add_top(g, v);
        }
    }
};
}
