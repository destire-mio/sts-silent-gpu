#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct GoldShrine : EventHooks {
    SPIRE_STATE(GoldShrine, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.GoldShrine", true}; }
    SPIRE_SNAPSHOT("GoldShrine.goldAmt", scalar, 50)
    SPIRE_SNAPSHOT("GoldShrine.screen", scalar, event.state.GoldShrine.screen ? "COMPLETE" : "INTRO")
    static SPIRE_FN void enter(Game& g) { event_options(g, 3); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.GoldShrine;
        if (s.screen) { open_map(g); return; }
        if (option == 0) gain_gold(g, 50);
        else if (option == 1) { gain_gold(g, 275); queue_card_obtain(g, make_card(CardId::Regret)); }
        s.screen = 1; event_options(g, 1);
    }
};
}
