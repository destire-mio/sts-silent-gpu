#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ForeignInfluence : ActionHooks {
    SPIRE_WAITING_RULE("ForeignInfluenceAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(ForeignInfluence, bool retrieveCard; bool upgraded; bool selecting;)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (!e.payload.state.ForeignInfluence.selecting) {
            g.reward_cards.clear();
            while (g.reward_cards.size != 3) {
                const CardId id = foreign_attack_card(g);
                bool duplicate = false;
                for (int i = 0; i < g.reward_cards.size; ++i)
                    if (g.cards[g.reward_cards[i]].id == id)
                        duplicate = true;
                if (!duplicate)
                    g.reward_cards.push_back(create_card(g, make_card(id)));
            }
            g.discovery_card = -1;
            g.reward_skippable = true;
            g.screen = Screen::CardReward;
            e.payload.state.ForeignInfluence.selecting = true;
            return;
        }
        if (!e.payload.state.ForeignInfluence.retrieveCard) {
            if (g.discovery_card >= 0) {
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
