#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct CursedTome : EventHooks {
    SPIRE_STATE(CursedTome, int finalDmg; int damageTaken; uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.CursedTome", true}; }
    SPIRE_SNAPSHOT("CursedTome.screen", scalar, event.state.CursedTome.screen == 0 ? "INTRO" : event.state.CursedTome.screen == 1 ? "PAGE_1" : event.state.CursedTome.screen == 2 ? "PAGE_2" : event.state.CursedTome.screen == 3 ? "PAGE_3" : event.state.CursedTome.screen == 4 ? "LAST_PAGE" : "END")
    static SPIRE_FN void enter(Game& g) {
        g.event.no_cards = true; g.event.state.CursedTome.finalDmg = 15; event_options(g, 2);
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.CursedTome;
        if (s.screen == 5) { open_map(g); return; }
        if (s.screen == 0) s.screen = option == 0 ? 1 : 5;
        else if (s.screen < 4) {
            const int damage = s.screen;
            event_damage(g, damage, no_target, DamageType::HpLoss); s.damageTaken += damage; ++s.screen;
        } else {
            const int damage = option == 0 ? s.finalDmg : 3;
            event_damage(g, damage, no_target, DamageType::HpLoss); s.damageTaken += damage;
            if (option == 0) {
                RelicId books[3]{}; int count = 0;
                if (!has_relic(g, RelicId::Necronomicon)) books[count++] = RelicId::Necronomicon;
                if (!has_relic(g, RelicId::Enchiridion)) books[count++] = RelicId::Enchiridion;
                if (!has_relic(g, RelicId::NilrysCodex)) books[count++] = RelicId::NilrysCodex;
                if (count == 0) books[count++] = RelicId::Circlet;
                RoomReward reward{}; reward.kind = RewardKind::Relic;
                reward.relic.id = books[g.rng.misc.random(count - 1)];
                g.room_rewards.clear(); g.room_rewards.push_back(create_reward(g, reward));
                g.room_phase = RoomPhase::Complete; open_combat_rewards(g);
            }
            s.screen = 5;
        }
        event_options(g, s.screen == 4 ? 2 : 1);
    }
};
}
