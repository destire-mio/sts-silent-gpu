#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct MysteriousSphere : EventHooks {
    SPIRE_STATE(MysteriousSphere, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.beyond.MysteriousSphere", false, true, nullptr, "2 Orb Walkers"}; }
    SPIRE_SNAPSHOT("MysteriousSphere.screen", scalar, event.state.MysteriousSphere.screen == 0 ? "INTRO" : event.state.MysteriousSphere.screen == 1 ? "PRE_COMBAT" : "END")
    static SPIRE_FN void enter(Game& g) { event_options(g, 2); construct_encounter(g, EncounterId::TwoOrbWalkers); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.MysteriousSphere;
        if (s.screen == 0) { s.screen = option == 0 ? 1 : 2; event_options(g, 1); }
        else if (s.screen == 1) {
            add_gold_reward(g, g.rng.misc.random(45, 55));
            RoomReward reward{}; reward.kind = RewardKind::Relic;
            reward.relic.id = random_screenless_relic(g, RelicTier::Rare);
            g.room_rewards.push_back(create_reward(g, reward));
            enter_combat(g); g.combat_metric_event = g.event.id;
        } else open_map(g);
    }
};
}
