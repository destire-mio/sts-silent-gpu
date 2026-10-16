#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Nloth : EventHooks {
    SPIRE_STATE(Nloth, int screenNum; uint8_t choice1; uint8_t choice2; uint8_t gift;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.Nloth", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.act == 2 && g.run.relics.size >= 2; }
    SPIRE_SNAPSHOT("Nloth.choice1", scalar, graph.retained_relic(event.state.Nloth.choice1, true))
    SPIRE_SNAPSHOT("Nloth.choice2", scalar, graph.retained_relic(event.state.Nloth.choice2, true))
    SPIRE_SNAPSHOT("Nloth.gift", scalar, graph.retained_relic(event.state.Nloth.gift, game.reward_objects[event.state.Nloth.gift].relic_slot >= 0))
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.Nloth;
        List<uint8_t, 64> relics{};
        for (int i = 0; i < g.run.relics.size; ++i) relics.push_back(i);
        shuffle(relics.values, relics.size, g.rng.misc);
        for (int i = 0; i < 2; ++i) {
            RoomReward object{}; object.kind = RewardKind::Relic;
            object.relic.id = g.run.relics[relics[i]].id; object.relic_slot = relics[i];
            const int reference = create_reward(g, object);
            g.reward_objects[reference].relic = g.run.relics[relics[i]];
            if (i == 0) s.choice1 = reference; else s.choice2 = reference;
        }
        RoomReward gift{}; gift.kind = RewardKind::Relic; gift.relic.id = RelicId::NlothsGift;
        s.gift = create_reward(g, gift);
        event_options(g, 3);
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Nloth;
        if (s.screenNum) { open_map(g); return; }
        if (option < 2) {
            const RelicId gift = has_relic(g, RelicId::NlothsGift) ? RelicId::Circlet : RelicId::NlothsGift;
            // Nloth.buttonEffect keeps the offered relic when the player already has the gift.
            if (gift == RelicId::NlothsGift)
                unequip_relic(g, g.reward_objects[option == 0 ? s.choice1 : s.choice2].relic.id);
            if (gift == RelicId::Circlet) {
                RoomReward object{}; object.kind = RewardKind::Relic; object.relic.id = gift;
                s.gift = create_reward(g, object);
            }
            obtain_relic_object(g, s.gift);
        }
        s.screenNum = 1; event_options(g, 1);
    }
};
}
