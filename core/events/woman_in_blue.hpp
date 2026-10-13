#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct WomanInBlue : EventHooks {
    SPIRE_STATE(WomanInBlue, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.WomanInBlue", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.gold >= 50; }
    SPIRE_SNAPSHOT("WomanInBlue.screen", scalar, event.state.WomanInBlue.screen == 0 ? "INTRO" : "RESULT")
    static SPIRE_FN void enter(Game& g) { g.event.no_cards = true; event_options(g, 4); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.WomanInBlue;
        if (s.screen != 0) { open_map(g); return; }
        if (option < 3) {
            lose_gold(g, 20 + option * 10); g.room_rewards.clear();
            for (int i = 0; i <= option; ++i) add_potion_reward(g, random_potion_from_pool(g, g.rng.potion));
            g.room_phase = RoomPhase::Complete; open_room_card_rewards(g);
            g.reward_proceed_visible = true; g.combat_reward_skippable = false;
        } else {
            DamageInfo info{}; info.source = no_target; info.target = player_target;
            info.amount = -floor_int(-g.player.creature.max_hp * 0.05f); info.damage_type = DamageType::HpLoss;
            player_damage(g, info);
        }
        event_options(g, 1); s.screen = 1;
    }
};
}
