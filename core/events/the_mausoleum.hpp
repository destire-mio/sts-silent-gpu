#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct TheMausoleum : EventHooks {
    SPIRE_STATE(TheMausoleum, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.TheMausoleum", true}; }
    SPIRE_SNAPSHOT("TheMausoleum.percent", scalar, 100)
    SPIRE_SNAPSHOT("TheMausoleum.screen", scalar, event.state.TheMausoleum.screen ? "RESULT" : "INTRO")
    static SPIRE_FN void enter(Game& g) { event_options(g, 2); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.TheMausoleum;
        if (s.screen) { open_map(g); return; }
        if (option == 0) {
            // TheMausoleum.buttonEffect rolls before A15 forces the curse.
            g.rng.misc.randomBoolean();
            queue_card_obtain(g, make_card(CardId::Writhe));
            equip_relic(g, random_screenless_relic(g, random_relic_tier(g)));
        }
        s.screen = 1; event_options(g, 1);
    }
};
}
