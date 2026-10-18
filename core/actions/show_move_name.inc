#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct ShowMoveName : ActionHooks {
    SPIRE_SNAPSHOT("ShowMoveNameAction.msg", scalar, game.text[effect.payload.state.ShowMoveName.msg])
    static SPIRE_FN constexpr ActionData data() { return {false, "TEXT"}; }
    SPIRE_STATE(ShowMoveName, uint16_t msg; bool explicit_message;)
    SPIRE_FN static void prepare(Game& g, Effect& e) {
        e.target = e.source;
        if (!e.payload.state.ShowMoveName.explicit_message) {
            Monster& monster = g.monsters[e.source];
            e.payload.state.ShowMoveName.msg = monster.move_name;
            monster.move_name = 0;
        }
    }
    SPIRE_FN static void execute(Game&, const Effect&) {}
};
}
