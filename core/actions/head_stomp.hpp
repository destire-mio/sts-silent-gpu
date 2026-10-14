#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct HeadStomp : ActionHooks {
    SPIRE_SNAPSHOT("HeadStompAction.m", creature, effect.target)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("HeadStompAction.magicNumber", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (g.manager.played_cards.size >= 2 &&
            card_data(g.cards[g.manager.played_cards.values[g.manager.played_cards.size - 2]].id).type ==
                CardType::Attack) {
            Effect a{};
            a.kind = EffectKind::ApplyPower;
            a.target = e.target;
            a.amount = e.extra;
            a.power = PowerId::Weak;
            add_top(g, a);
        }
    }
};
}
