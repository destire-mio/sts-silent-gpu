#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct MaskedBandits : EventHooks {
    SPIRE_STATE(MaskedBandits, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.MaskedBandits", false, true}; }
    SPIRE_SNAPSHOT("MaskedBandits.screen", scalar, event.state.MaskedBandits.screen == 0 ? "INTRO" : event.state.MaskedBandits.screen == 1 ? "PAID_1" : event.state.MaskedBandits.screen == 2 ? "PAID_2" : event.state.MaskedBandits.screen == 3 ? "PAID_3" : "END")
    static SPIRE_FN void enter(Game& g) { event_add_option(g); event_add_option(g); construct_encounter(g, EncounterId::MaskedBandits); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.MaskedBandits;
        if (s.screen == 0) {
            if (option == 0) {
                // MaskedBandits.stealGold gives each GainPennyEffect a random monster owner.
                for (int i = 0; i < g.run.gold; ++i) ++g.monsters[g.monster_order[g.unseeded.random(0, g.monster_order.size - 1)]].creature.gold;
                lose_gold(g, g.run.gold); s.screen = 1; event_options(g, 1);
            }
            else if (option == 1) {
                add_gold_reward(g, g.rng.misc.random(25, 35));
                RoomReward reward{}; reward.kind = RewardKind::Relic;
                reward.relic.id = has_relic(g, RelicId::RedMask) ? RelicId::Circlet : RelicId::RedMask;
                g.room_rewards.push_back(create_reward(g, reward));
                enter_combat(g);
            }
        } else if (s.screen < 3) ++s.screen;
        else { s.screen = 4; open_map(g); }
    }
};
}
