#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct SetDontTrigger : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("SetDontTriggerAction.card", card, effect.card)
    SPIRE_SNAPSHOT("SetDontTriggerAction.trigger", scalar, effect.flag)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        g.cards[e.card].dont_trigger_on_use_card = e.flag;
    }
};
}
