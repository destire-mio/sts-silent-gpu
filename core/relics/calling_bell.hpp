#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct CallingBell : RelicHooks {
    SPIRE_STATE(CallingBell, bool cardsReceived;)
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void initializeState(Relic& relic) { relic.state.CallingBell.cardsReceived = true; }
    static SPIRE_FN void onEquip(Game& g, int relic) {
        g.run.relics[relic].state.CallingBell.cardsReceived = false;
        g.confirmation_card = CardId::CurseOfTheBell;
        g.screen = Screen::MasterConfirm;
    }
    static SPIRE_FN RelicKey screenlessRelic(Game& g, int tier) {
        RelicKey key;
        do { key = random_relic_key(g, tier); }
        while (key == RelicKey::BottledFlame || key == RelicKey::BottledLightning || key == RelicKey::BottledTornado || key == RelicKey::Whetstone);
        return key;
    }
    static SPIRE_FN void update(Game& g, int relic) {
        if (!g.run.relics[relic].state.CallingBell.cardsReceived && g.screen == Screen::None) {
            List<CardId, 128> cards{};
            card_reward_ids(g, cards);
            g.acquisition_relics.clear();
            for (int tier = 0; tier < 3; ++tier) g.acquisition_relics.push_back(screenlessRelic(g, tier));
            g.screen = Screen::RelicRewards;
            g.run.relics[relic].state.CallingBell.cardsReceived = true;
        }
    }
};
}
