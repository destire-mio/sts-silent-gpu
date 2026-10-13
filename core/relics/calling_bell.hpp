#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct CallingBell : RelicHooks {
    SPIRE_STATE(CallingBell, bool cardsReceived;)
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void initializeState(Relic& relic) { relic.state.CallingBell.cardsReceived = true; }
    static SPIRE_FN void onEquip(Game& g, int relic) {
        g.run.relics[relic].state.CallingBell.cardsReceived = false;
        g.grid_cards.clear();
        g.grid_cards.push_back(create_card(g, make_card(CardId::CurseOfTheBell)));
        g.selection_confirmation = true;
        g.screen = Screen::Grid;
    }
    static SPIRE_FN RelicKey screenlessRelic(Game& g, int tier) {
        RelicKey key;
        do { key = random_relic_key(g, tier); }
        while (key == RelicKey::BottledFlame || key == RelicKey::BottledLightning || key == RelicKey::BottledTornado || key == RelicKey::Whetstone);
        return key;
    }
    static SPIRE_FN void update(Game& g, int relic) {
        if (!g.run.relics[relic].state.CallingBell.cardsReceived && g.screen == Screen::None) {
            add_room_card_reward(g);
            g.room_rewards.clear();
            for (int tier = 0; tier < 3; ++tier) {
                RoomReward reward{};
                reward.kind = RewardKind::Relic;
                reward.relic = relic_id(relic_key_name(screenlessRelic(g, tier)));
                reward.identity = g.next_reward_identity++;
                g.room_rewards.push_back(reward);
            }
            g.reward_proceed_visible = true;
            g.reward_skippable = false;
            g.screen = Screen::CombatReward;
            g.run.relics[relic].state.CallingBell.cardsReceived = true;
        }
    }
};
}
