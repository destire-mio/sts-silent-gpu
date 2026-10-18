#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct SecretPortal : EventHooks {
    SPIRE_STATE(SecretPortal, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.beyond.SecretPortal", true}; }
    SPIRE_SNAPSHOT("SecretPortal.screen", scalar, event.state.SecretPortal.screen == 0 ? "INTRO" : event.state.SecretPortal.screen == 1 ? "ACCEPT" : "LEAVE")
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.act == 3 && g.run.profile.playtime_at_least_800; }
    static SPIRE_FN void enter(Game& g) { event_options(g, 2); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.SecretPortal;
        if (s.screen == 0) { s.screen = option == 0 ? 1 : 2; event_options(g, 1); }
        else if (s.screen == 1) {
            g.run.path_x.push_back(1); g.run.path_y.push_back(15);
            g.run.next_x = g.run.node_x = -1; g.run.next_y = g.run.node_y = 15;
            g.run.node_on_map = false;
            ++g.run.floor;
            enter_room(g, RoomKind::Boss);
        } else open_map(g);
    }
};
}
