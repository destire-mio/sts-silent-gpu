#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct EscapePlanAction : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    SPIRE_SNAPSHOT("EscapePlanAction.blockGain", scalar, effect.amount)
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        for (int i = 0; i < g.drawn_cards.size; ++i) {
            if (card_data(g.cards[g.drawn_cards[i]].id).type != CardType::Skill) continue;
            Effect block{};
            block.kind = EffectKind::Block;
            block.source = block.target = player_target;
            block.amount = e.amount;
            add_top(g, block);
            break;
        }
        e.done = true;
    }
};
}
