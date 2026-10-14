#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Nest : EventHooks {
    SPIRE_STATE(Nest, uint8_t screenNum;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.Nest", true}; }
    SPIRE_SNAPSHOT("Nest.goldGain", scalar, 50)
    static SPIRE_FN void enter(Game& g) { event_options(g, 1); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Nest;
        if (s.screenNum == 0) { s.screenNum = 1; event_options(g, 2); }
        else if (s.screenNum == 1) {
            if (option == 0) gain_gold(g, 50);
            else { event_damage(g, 6); queue_card_obtain(g, make_card(CardId::RitualDagger)); }
            s.screenNum = 2; event_options(g, 1);
        } else open_map(g);
    }
};
}
