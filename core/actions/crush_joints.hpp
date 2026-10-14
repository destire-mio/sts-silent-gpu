#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct CrushJoints : ActionHooks {
    SPIRE_SNAPSHOT("CrushJointsAction.m", creature, effect.target)
    SPIRE_SNAPSHOT("CrushJointsAction.magicNumber", scalar, effect.extra)
    SPIRE_SNAPSHOT("AbstractGameAction.target", creature, no_target)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (g.manager.played_cards.size >= 2 &&
            card_data(g.cards[g.manager.played_cards.values[g.manager.played_cards.size - 2]].id).type ==
                CardType::Skill) {
            Effect a{};
            a.kind = EffectKind::ApplyPower;
            a.target = e.target;
            a.amount = e.extra;
            a.power = PowerId::Vulnerable;
            add_top(g, a);
        }
    }
};
}
