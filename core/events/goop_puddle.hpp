#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct GoopPuddle : EventHooks {
    SPIRE_STATE(GoopPuddle, int goldLoss; uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.GoopPuddle", true}; }
    SPIRE_SNAPSHOT("GoopPuddle.damage", scalar, 11)
    SPIRE_SNAPSHOT("GoopPuddle.gold", scalar, 75)
    SPIRE_SNAPSHOT("GoopPuddle.screen", scalar, event.state.GoopPuddle.screen ? "RESULT" : "INTRO")
    static SPIRE_FN void enter(Game& g) { g.event.state.GoopPuddle.goldLoss = min(g.run.gold, g.rng.misc.random(35, 75)); event_options(g, 2); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.GoopPuddle;
        if (s.screen) { open_map(g); return; }
        if (option == 0) { event_damage(g, 11, player_target); gain_gold(g, 75); }
        else lose_gold(g, s.goldLoss);
        event_options(g, 1); s.screen = 1;
    }
};
}
