#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ShowMoveName : ActionHooks {
    SPIRE_SNAPSHOT("ShowMoveNameAction.msg", scalar, effect.state.ShowMoveName.has_msg ? effect.state.ShowMoveName.msg : nullptr)
    static SPIRE_FN constexpr ActionData data() { return {false, "TEXT"}; }
    SPIRE_STATE(ShowMoveName, char msg[128]; bool has_msg; bool explicit_message;)
    SPIRE_FN static void prepare(Game& g, Effect& e) {
        e.target = e.source;
        if (!e.state.ShowMoveName.explicit_message) {
            Monster& monster = g.monsters[e.source];
            e.state.ShowMoveName.has_msg = monster.move_name_present;
            for (int i = 0; i < 128; ++i) e.state.ShowMoveName.msg[i] = monster.move_name[i];
            monster.move_name_present = false;
        }
    }
    SPIRE_FN static void execute(Game&, const Effect&) {}
};
}
