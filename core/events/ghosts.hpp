#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Ghosts : EventHooks {
    SPIRE_STATE(Ghosts, int hpLoss; int screenNum;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.Ghosts", true}; }
    static SPIRE_FN void enter(Game& g) {
        g.event.state.Ghosts.hpLoss = min(-floor_int(-(g.player.creature.max_hp * 0.5f)), g.player.creature.max_hp - 1);
        event_options(g, 2);
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Ghosts;
        if (s.screenNum) { open_map(g); return; }
        if (option == 0) {
            decrease_max_hp(g, s.hpLoss);
            for (int i = 0; i < 3; ++i) queue_card_obtain(g, make_card(CardId::Apparition));
            s.screenNum = 1;
        } else s.screenNum = 2;
        event_options(g, 1);
    }
};
}
