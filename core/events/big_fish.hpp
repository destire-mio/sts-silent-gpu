#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct BigFish : EventHooks {
    SPIRE_STATE(BigFish, int healAmt; uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.BigFish", true}; }
    SPIRE_SNAPSHOT("BigFish.screen", scalar, event.state.BigFish.screen ? "RESULT" : "INTRO")
    static SPIRE_FN void enter(Game& g) { g.event.state.BigFish.healAmt = g.player.creature.max_hp / 3; event_options(g, 3); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.BigFish;
        if (s.screen) { open_map(g); return; }
        if (option == 0) heal_creature(g, player_target, s.healAmt);
        else if (option == 1) increase_max_hp(g, 5);
        else {
            const RelicId relic = random_screenless_relic(g, random_relic_tier(g));
            queue_card_obtain(g, make_card(CardId::Regret));
            equip_relic(g, relic);
        }
        event_options(g, 1); s.screen = 1;
    }
};
}
