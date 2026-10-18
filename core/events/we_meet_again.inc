#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct WeMeetAgain : EventHooks {
    SPIRE_STATE(WeMeetAgain, uint16_t cardOption; uint8_t potionOption; uint8_t givenRelic; int goldAmount; uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.WeMeetAgain", true, false, nullptr, nullptr, true}; }
    SPIRE_SNAPSHOT("WeMeetAgain.screen", scalar, event.state.WeMeetAgain.screen ? "COMPLETE" : "INTRO")
    SPIRE_SNAPSHOT("WeMeetAgain.cardOption", scalar, graph.master_card_uuid(event.state.WeMeetAgain.cardOption))
    SPIRE_SNAPSHOT("WeMeetAgain.potionOption", scalar, event.state.WeMeetAgain.potionOption ? graph.core_reward_potion(event.state.WeMeetAgain.potionOption - 1) : pybind11::object(pybind11::none()))
    SPIRE_SNAPSHOT("WeMeetAgain.givenRelic", scalar, event.state.WeMeetAgain.givenRelic ? graph.retained_relic(event.state.WeMeetAgain.givenRelic - 1, game.reward_objects[event.state.WeMeetAgain.givenRelic - 1].relic_slot >= 0) : pybind11::object(pybind11::none()))
    static SPIRE_FN bool referencesMasterCard(const Game& g, uint32_t uuid) { return g.event.state.WeMeetAgain.cardOption == uuid; }
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.WeMeetAgain;
        List<uint8_t, potion_capacity> potions{};
        for (int i = 0; i < g.run.potion_slots; ++i)
            if (g.run.potions[i] != PotionId::Empty) potions.push_back(i);
        if (!potions.empty()) {
            shuffle(potions.values, potions.size, g.rng.misc);
            const int slot = potions[0];
            RoomReward object{}; object.kind = RewardKind::Potion; object.potion = g.run.potions[slot];
            object.potion_state = g.run.potion_state[slot]; object.potion_slot = slot;
            s.potionOption = create_reward(g, object) + 1;
            g.run.potion_identity[slot] = g.reward_objects[s.potionOption - 1].identity;
        }
        if (g.run.gold >= 50) s.goldAmount = g.rng.misc.random(50, min(150, g.run.gold));
        Pile cards{};
        for (int i = 0; i < g.run.master_deck.size; ++i)
            if (card_rarity(g.run.master_deck[i].id) != CardRarity::Basic && card_def(g.run.master_deck[i].id).type != CardType::Curse)
                cards.push_back(i);
        if (!cards.empty()) { shuffle(cards.values, cards.size, g.rng.misc); s.cardOption = g.run.master_deck[cards[0]].uuid; }
        event_options(g, 4);
        g.image_dialog[0].disabled = s.potionOption == 0; g.image_dialog[1].disabled = s.goldAmount == 0; g.image_dialog[2].disabled = s.cardOption == 0;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.WeMeetAgain;
        if (s.screen) { open_map(g); return; }
        s.screen = 1;
        if (option == 0) {
            const int slot = g.reward_objects[s.potionOption - 1].potion_slot;
            g.run.potions[slot] = PotionId::Empty; g.run.potion_identity[slot] = 0; potion_initialize_state(g, slot);
        } else if (option == 1) lose_gold(g, s.goldAmount);
        else if (option == 2) {
            int index = 0;
            while (g.run.master_deck[index].uuid != s.cardOption) ++index;
            remove_master_card(g, index);
        }
        if (option != 3) {
            RoomReward object{}; object.kind = RewardKind::Relic;
            object.relic.id = random_screenless_relic(g, random_relic_tier(g));
            s.givenRelic = create_reward(g, object) + 1;
            obtain_relic_object(g, s.givenRelic - 1);
        }
        event_options(g, 1);
    }
};
}
