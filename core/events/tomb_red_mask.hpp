#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct TombRedMask : EventHooks {
    SPIRE_STATE(TombRedMask, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.beyond.TombRedMask", true}; }
    SPIRE_SNAPSHOT("TombRedMask.screen", scalar, event.state.TombRedMask.screen ? "RESULT" : "INTRO")
    static SPIRE_FN void enter(Game& g) {
        const bool masked = has_relic(g, "Red Mask");
        event_options(g, masked ? 2 : 3); g.image_dialog[0].disabled = !masked;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.TombRedMask;
        if (s.screen) { open_map(g); return; }
        if (option == 0) gain_gold(g, 222);
        else if (option == 1 && !has_relic(g, "Red Mask")) { lose_gold(g, g.run.gold); equip_relic(g, RelicId::RedMask); }
        else open_map(g);
        event_options(g, 1); s.screen = 1;
    }
};
}
