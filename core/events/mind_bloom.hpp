#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct MindBloom : EventHooks {
    SPIRE_STATE(MindBloom, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.beyond.MindBloom", true, true}; }
    SPIRE_SNAPSHOT("MindBloom.screen", scalar, event.state.MindBloom.screen == 0 ? "INTRO" : event.state.MindBloom.screen == 1 ? "FIGHT" : "LEAVE")
    static SPIRE_FN void enter(Game& g) { event_options(g, 3); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.MindBloom;
        if (s.screen) { open_map(g); return; }
        if (option == 0) {
            s.screen = 1;
            EncounterId bosses[] = {EncounterId::TheGuardian, EncounterId::Hexaghost, EncounterId::SlimeBoss};
            shuffle(bosses, 3, g.rng.misc);
            construct_encounter(g, bosses[0]);
            g.room_rewards.clear(); add_gold_reward(g, 25); add_relic_reward(g, RelicTier::Rare);
            begin_combat(g);
            g.last_combat_metric = g.text.intern("Mind Bloom Boss Battle");
        } else {
            s.screen = 2;
            if (option == 1) {
                for (int i = 0; i < g.run.master_deck.size; ++i)
                    if (card_canUpgrade(g, -i - 2)) card_upgrade(g.run.master_deck[i]);
                equip_relic(g, RelicId::MarkOfTheBloom);
            } else if (g.run.floor % 50 <= 40) {
                gain_gold(g, 999);
                queue_card_obtain(g, make_card(CardId::Normality)); queue_card_obtain(g, make_card(CardId::Normality));
            } else { heal_creature(g, player_target, g.player.creature.max_hp); queue_card_obtain(g, make_card(CardId::Doubt)); }
        }
        event_options(g, 1);
    }
};
}
