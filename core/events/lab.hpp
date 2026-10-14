#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Lab : EventHooks {
    SPIRE_STATE(Lab, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.Lab", true, true}; }
    SPIRE_SNAPSHOT("Lab.screen", scalar, event.state.Lab.screen ? "COMPLETE" : "INTRO")
    static SPIRE_FN void enter(Game& g) { g.event.no_cards = true; event_options(g, 1); }
    static SPIRE_FN void choose(Game& g, int) {
        auto& s = g.event.state.Lab;
        if (s.screen) { open_map(g); return; }
        g.room_rewards.clear();
        for (int i = 0; i < 2; ++i) add_potion_reward(g, random_potion_from_pool(g, g.rng.potion));
        s.screen = 1; g.room_phase = RoomPhase::Complete;
        open_room_card_rewards(g);
        g.reward_proceed_visible = true; g.combat_reward_skippable = false;
    }
};
}
