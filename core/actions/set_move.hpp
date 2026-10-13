#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct SetMove : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("SetMoveAction.monster", monster, effect.target)
    SPIRE_SNAPSHOT("SetMoveAction.theNextDamage", scalar, effect.card)
    SPIRE_SNAPSHOT("SetMoveAction.theNextIntent", intent, effect.extra)
    SPIRE_SNAPSHOT("SetMoveAction.theNextMove", scalar, effect.amount)
    SPIRE_SNAPSHOT("SetMoveAction.theNextName", scalar, game.text[effect.payload.state.SetMove.the_next_name])
    SPIRE_STATE(SetMove, int the_multiplier; bool is_multiplier; uint16_t the_next_name;)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        monster_setMove(g, e.target, e.amount, static_cast<Intent>(e.extra), e.card, 1);
        Monster& m = g.monsters[e.target];
        m.pending_hits = e.payload.state.SetMove.the_multiplier;
        m.pending_multi = e.payload.state.SetMove.is_multiplier;
        m.move_name = e.payload.state.SetMove.the_next_name;
    }
};
}
