#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Colosseum : EventHooks {
    SPIRE_STATE(Colosseum, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.Colosseum", true, true}; }
    SPIRE_SNAPSHOT("Colosseum.screen", scalar, event.state.Colosseum.screen == 0 ? "INTRO" : event.state.Colosseum.screen == 1 ? "FIGHT" : event.state.Colosseum.screen == 2 ? "LEAVE" : "POST_COMBAT")
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.node_y > 7; }
    static SPIRE_FN void enter(Game& g) { event_options(g, 1); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Colosseum;
        if (s.screen == 0) { s.screen = 1; return; }
        if (s.screen == 1) {
            s.screen = 3; g.room_reward_allowed = false; g.room_rewards.clear();
            g.event.has_focus = false; prepare_room_combat(g, EncounterId::ColosseumSlavers); enter_combat(g); event_options(g, 1);
        } else if (s.screen == 3) {
            g.room_reward_allowed = true;
            if (option == 1) {
                s.screen = 2; g.room_rewards.clear();
                // Colosseum.buttonEffect constructs the encounter before rolling its relic rewards.
                g.event.has_focus = false; prepare_room_combat(g, EncounterId::ColosseumNobs);
                add_relic_reward(g, RelicTier::Rare); add_relic_reward(g, RelicTier::Uncommon); add_gold_reward(g, 100);
                g.room_elite_trigger = true;
                enter_combat(g);
            } else open_map(g);
        } else open_map(g);
    }
    static SPIRE_FN void reopen(Game& g) {
        if (g.event.state.Colosseum.screen == 2) return;
        g.player.orb_count = 0; g.player.creature.block = 0; g.player.creature.powers.clear();
        g.hand.clear(); g.draw.clear(); g.discard.clear(); g.exhaust.clear(); g.limbo.clear();
        g.player.creature.draw_x = 320.0f;
        prepare_player_battle(g);
        g.room_phase = RoomPhase::Event; g.room_in_combat = false; g.battle_over = false; g.monster_order.clear(); g.room_rewards.clear();
        g.event.has_focus = true; g.screen = Screen::Event; event_options(g, 2);
    }
};
}
