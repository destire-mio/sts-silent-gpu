#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct DeadAdventurer : EventHooks {
    SPIRE_STATE(DeadAdventurer, int numRewards; int encounterChance; int goldRewardMetric; uint8_t enemy; uint8_t screen; uint8_t count; uint8_t rewards[3]; uint8_t relicReward;)
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.DeadAdventurer", false, true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.floor > 6; }
    SPIRE_SNAPSHOT("DeadAdventurer.screen", scalar, event.state.DeadAdventurer.screen == 0 ? "INTRO" : event.state.DeadAdventurer.screen == 1 ? "FAIL" : event.state.DeadAdventurer.screen == 2 ? "SUCCESS" : "ESCAPE")
    SPIRE_SNAPSHOT("DeadAdventurer.relicRewardMetric", scalar, event.state.DeadAdventurer.relicReward ? graph.relic(event.state.DeadAdventurer.relicReward - 1) : pybind11::object(pybind11::none()))
    SPIRE_SNAPSHOT("DeadAdventurer.rewards", scalar, [&]() -> pybind11::object { pybind11::list result; const auto& s = event.state.DeadAdventurer; for (int i = 0; i < s.count; ++i) result.append(s.rewards[i] == 0 ? "GOLD" : s.rewards[i] == 1 ? "NOTHING" : "RELIC"); return result; }())
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.DeadAdventurer;
        s.count = 3; s.rewards[0] = 0; s.rewards[1] = 1; s.rewards[2] = 2;
        shuffle(s.rewards, s.count, g.rng.misc);
        s.encounterChance = 35; s.enemy = g.rng.misc.random(0, 2);
        event_add_option(g); event_add_option(g);
    }
    static SPIRE_FN EncounterId encounter(const Game& g) {
        const int enemy = g.event.state.DeadAdventurer.enemy;
        return enemy == 0 ? EncounterId::ThreeSentries : enemy == 1 ? EncounterId::GremlinNob : EncounterId::LagavulinEvent;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.DeadAdventurer;
        if (s.screen == 1) {
            for (int i = 0; i < s.count; ++i) {
                if (s.rewards[i] == 0) add_gold_reward(g, 30);
                else if (s.rewards[i] == 2) add_relic_reward(g, random_relic_tier(g));
            }
            ++s.numRewards;
            enter_event_combat(g, encounter(g));
        } else if (s.screen == 2 || s.screen == 3) open_map(g);
        else if (option == 1) { s.screen = 3; event_remove_option(g, 1); }
        else if (option != 0) return;
        else if (g.rng.misc.random(0, 99) < s.encounterChance) {
            s.screen = 1; event_remove_option(g, 1);
            add_gold_reward(g, g.rng.misc.random(25, 35));
            build_encounter(g, encounter(g));
            g.room_elite_trigger = true;
        } else {
            ++s.numRewards; s.encounterChance += 25;
            const int reward = s.rewards[0];
            for (int i = 1; i < s.count; ++i) s.rewards[i - 1] = s.rewards[i];
            --s.count;
            if (reward == 0) { gain_gold(g, 30); s.goldRewardMetric = 30; }
            else if (reward == 2) {
                s.relicReward = g.run.relics.size + 1;
                equip_relic(g, random_screenless_relic(g, random_relic_tier(g)));
            }
            if (s.numRewards == 3) { s.screen = 2; event_remove_option(g, 1); }
        }
    }
};
}
