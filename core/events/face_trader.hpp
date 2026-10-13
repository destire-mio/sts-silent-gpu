#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct FaceTrader : EventHooks {
    SPIRE_STATE(FaceTrader, uint8_t screen;)
    SPIRE_STATIC_STATE(FaceTrader, int goldReward; int damage;)
    SPIRE_STATIC("goldReward", game.unit_statics.FaceTrader.goldReward)
    SPIRE_STATIC("damage", game.unit_statics.FaceTrader.damage)
    SPIRE_SNAPSHOT("FaceTrader.screen", scalar, event.state.FaceTrader.screen == 0 ? "INTRO" : event.state.FaceTrader.screen == 1 ? "MAIN" : "RESULT")
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.FaceTrader", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.act == 1 || g.run.act == 2; }
    static SPIRE_FN void enter(Game& g) {
        g.unit_statics.FaceTrader.goldReward = 50;
        g.unit_statics.FaceTrader.damage = max(1, g.player.creature.max_hp / 10);
        event_options(g, 1);
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.FaceTrader;
        if (s.screen == 0) { s.screen = 1; event_options(g, 3); }
        else if (s.screen == 1) {
            if (option == 0) {
                gain_gold(g, g.unit_statics.FaceTrader.goldReward);
                event_damage(g, g.unit_statics.FaceTrader.damage);
            } else if (option == 1) {
                const RelicId faces[] = {RelicId::CultistMask, RelicId::FaceOfCleric, RelicId::GremlinMask, RelicId::NlothsMask, RelicId::SsserpentHead};
                List<RelicId, 5> choices{};
                for (const auto face : faces) {
                    bool owned = false;
                    for (int i = 0; i < g.run.relics.size; ++i) owned |= g.run.relics[i].id == face;
                    if (!owned) choices.push_back(face);
                }
                if (choices.empty()) choices.push_back(RelicId::Circlet);
                shuffle(choices.values, choices.size, g.rng.misc);
                equip_relic(g, choices[0]);
            }
            s.screen = 2; event_options(g, 1);
        } else open_map(g);
    }
};
}
