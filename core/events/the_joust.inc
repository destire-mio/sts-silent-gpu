#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct TheJoust : EventHooks {
    SPIRE_STATE(TheJoust, uint8_t screen; bool betFor; bool ownerWins;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.TheJoust", true}; }
    SPIRE_SNAPSHOT("TheJoust.screen", scalar, event.state.TheJoust.screen == 0 ? "HALT" : event.state.TheJoust.screen == 1 ? "EXPLANATION" : event.state.TheJoust.screen == 2 ? "PRE_JOUST" : event.state.TheJoust.screen == 3 ? "JOUST" : "COMPLETE")
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.act == 2 && g.run.gold >= 50; }
    static SPIRE_FN void enter(Game& g) { event_options(g, 1); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.TheJoust;
        if (s.screen == 0) event_options(g, 2);
        else if (s.screen == 1) { s.betFor = option != 0; lose_gold(g, 50); event_options(g, 1); }
        else if (s.screen == 2) s.ownerWins = g.rng.misc.randomBoolean(0.3f);
        else if (s.screen == 3) { if (s.betFor == s.ownerWins) gain_gold(g, s.betFor ? 250 : 100); }
        else { open_map(g); return; }
        ++s.screen;
    }
};
}
