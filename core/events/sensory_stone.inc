#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct SensoryStone : EventHooks {
    SPIRE_STATE(SensoryStone, int choice; uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.beyond.SensoryStone", true}; }
    SPIRE_SNAPSHOT("SensoryStone.screen", scalar, event.state.SensoryStone.screen == 0 ? "INTRO" : event.state.SensoryStone.screen == 1 ? "INTRO_2" : event.state.SensoryStone.screen == 2 ? "ACCEPT" : "LEAVE")
    static SPIRE_FN void enter(Game& g) { g.event.no_cards = true; event_options(g, 1); }
    static SPIRE_FN void reward(Game& g) {
        g.room_rewards.clear();
        for (int i = 0; i < g.event.state.SensoryStone.choice; ++i) g.room_rewards.push_back(create_colorless_reward(g));
        g.room_phase = RoomPhase::Complete;
        open_room_card_rewards(g);
        g.reward_proceed_visible = true; g.combat_reward_skippable = false;
        g.event.state.SensoryStone.screen = 3;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.SensoryStone;
        if (s.screen == 0) { event_options(g, 3); s.screen = 1; }
        else if (s.screen == 1) {
            g.rng.misc.randomLong();
            s.screen = 2; s.choice = option + 1;
            reward(g);
            if (option > 0) {
                DamageInfo damage{}; damage.source = no_target; damage.target = player_target;
                damage.amount = option * 5; damage.damage_type = DamageType::HpLoss;
                player_damage(g, damage);
            }
            event_options(g, 1);
        } else { if (s.screen == 2) reward(g); open_map(g); }
    }
};
}
