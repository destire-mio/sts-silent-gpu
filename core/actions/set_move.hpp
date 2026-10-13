#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct SetMove : ActionHooks {
    SPIRE_STATE(SetMove, int the_multiplier; bool is_multiplier; char the_next_name[128]; bool has_name;)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        monster_setMove(g, e.target, e.amount, static_cast<Intent>(e.extra), e.card, 1);
        Monster& m = g.monsters[e.target];
        m.pending_hits = e.state.SetMove.the_multiplier;
        m.pending_multi = e.state.SetMove.is_multiplier;
        m.move_name_present = e.state.SetMove.has_name;
        for (int i = 0; i < 128; ++i) m.move_name[i] = e.state.SetMove.the_next_name[i];
    }
};
}
