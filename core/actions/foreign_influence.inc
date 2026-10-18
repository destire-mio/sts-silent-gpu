#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct ForeignInfluence : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(ForeignInfluence, bool retrieveCard; bool upgraded; bool selecting;)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (!e.payload.state.ForeignInfluence.selecting) {
            g.reward_cards.clear();
            while (g.reward_cards.size != 3) {
                const int library = foreign_attack_card(g);
                bool duplicate = false;
                for (int i = 0; i < g.reward_cards.size; ++i) {
                    const int card = g.reward_cards[i];
                    if (g.cards[card].id == library_card_native(library))
                        duplicate = true;
                }
                if (!duplicate)
                    g.reward_cards.push_back(create_library_card(g, library));
            }
            g.discovery_card = -1;
            g.reward_skippable = true;
            g.screen = Screen::CardReward;
            e.payload.state.ForeignInfluence.selecting = true;
            return;
        }
        if (!e.payload.state.ForeignInfluence.retrieveCard) {
            if (g.discovery_card >= 0) {
                SPIRE_ASSERT(g.discovery_card < card_capacity);
                Card copy = card_makeStatEquivalentCopy(g, g.discovery_card);
                if (e.payload.state.ForeignInfluence.upgraded && copy.cost_for_turn >= 0)
                    copy.cost_for_turn = 0;
                const int card = create_card(g, copy);
                if (g.hand.size < hand_limit)
                    add_created_card_to_hand(g, card);
                else {
                    upgrade_created_card(g, card);
                    g.discard.push_back(card);
                }
                g.discovery_card = -1;
            }
            e.payload.state.ForeignInfluence.retrieveCard = true;
        }
    }
};
}
