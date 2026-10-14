#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Mushrooms : EventHooks {
    SPIRE_STATE(Mushrooms, uint8_t screenNum;)
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.Mushrooms", false, true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.floor > 6; }
    static SPIRE_FN void enter(Game& g) { event_options(g, 2); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Mushrooms;
        if (s.screenNum == 0) {
            if (option == 0) { construct_encounter(g, EncounterId::MushroomLair); s.screenNum = 2; }
            else {
                heal_creature(g, player_target, static_cast<int>(g.player.creature.max_hp * 0.25f));
                queue_card_obtain(g, make_card(CardId::Parasite)); s.screenNum = 1;
            }
            event_options(g, 1);
        } else if (s.screenNum == 1) open_map(g);
        else {
            add_gold_reward(g, g.rng.misc.random(20, 30));
            RoomReward reward{}; reward.kind = RewardKind::Relic;
            reward.relic.id = has_relic(g, "Odd Mushroom") ? RelicId::Circlet : RelicId::OddMushroom;
            g.room_rewards.push_back(create_reward(g, reward));
            begin_combat(g);
            g.last_combat_metric = g.text.intern("The Mushroom Lair");
        }
    }
};
}
