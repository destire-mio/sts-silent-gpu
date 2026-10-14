#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Addict : EventHooks {
    SPIRE_STATE(Addict, int screenNum;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.Addict", true}; }
    static SPIRE_FN void enter(Game& g) {
        event_options(g, 3); g.image_dialog[0].disabled = g.run.gold < 85;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Addict;
        if (s.screenNum) { open_map(g); return; }
        if (option == 0 && g.run.gold < 85) { s.screenNum = 1; return; }
        if (option < 2) {
            const RelicId relic = random_screenless_relic(g, random_relic_tier(g));
            if (option == 0) lose_gold(g, 85);
            else queue_card_obtain(g, make_card(CardId::Shame));
            equip_relic(g, relic);
        }
        event_options(g, 1); s.screenNum = 1;
        if (option == 2) open_map(g);
    }
};
}
