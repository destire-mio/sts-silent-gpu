#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct ChooseOne : ActionHooks {
    SPIRE_STATE(ChooseOne, uint8_t choices[3]; uint8_t count; bool selecting;)
    SPIRE_SNAPSHOT("ChooseOneAction.choices", card_array, effect.payload.state.ChooseOne.choices, effect.payload.state.ChooseOne.count)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (!e.payload.state.ChooseOne.selecting) {
            g.reward_cards.clear();
            for (int i = 0; i < e.payload.state.ChooseOne.count; ++i)
                g.reward_cards.push_back(e.payload.state.ChooseOne.choices[i]);
            g.discovery_card = -1;
            g.reward_skippable = false;
            g.screen = Screen::CardReward;
            e.payload.state.ChooseOne.selecting = true;
            return;
        }
        if (g.discovery_card >= 0) {
            card_onChoseThisOption(g, g.discovery_card);
            g.discovery_card = -1;
        }
    }
};
}
