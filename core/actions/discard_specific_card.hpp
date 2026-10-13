#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DiscardSpecificCard : ActionHooks {
    SPIRE_SNAPSHOT("DiscardSpecificCardAction.targetCard", card, effect.card)
    SPIRE_SNAPSHOT("DiscardSpecificCardAction.group", optional_pile_group, effect.state.DiscardSpecificCard.group, !effect.state.DiscardSpecificCard.defaultGroup)
    static SPIRE_FN constexpr ActionData data() { return {false, "DISCARD"}; }
    SPIRE_STATE(DiscardSpecificCard, PileId group; bool defaultGroup;)
    SPIRE_FN static Effect make(int card, PileId group) {
        Effect e{};
        e.kind = EffectKind::DiscardSpecificCard;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.card = card;
        e.state.DiscardSpecificCard.group = group;
        e.state.DiscardSpecificCard.defaultGroup = false;
        return e;
    }
    SPIRE_FN static Effect make(int card) {
        Effect e = make(card, PileId::Hand);
        e.state.DiscardSpecificCard.defaultGroup = true;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        e.state.DiscardSpecificCard.defaultGroup = false;
        if (remove_from_pile(get_pile(g, e.state.DiscardSpecificCard.group), e.card)) {
            move_to_discard(g, e.card);
            increment_discard(g, false);
            card_triggerOnManualDiscard(g, e.card);
        }
    }
};
}
