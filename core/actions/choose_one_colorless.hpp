#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ChooseOneColorless : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(ChooseOneColorless, bool retrieve_card; bool opened;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.source = no_target;
        e.target = no_target;
        e.amount = 1;
    }
    SPIRE_FN static void generate_card_choices(Game& g) {
        g.reward_cards.clear();
        CardId chosen[3]{};
        while (g.reward_cards.size != 3) {
            const CardId name = random_card_in_combat(g, -1, true);
            bool duplicate = false;
            for (int i = 0; i < g.reward_cards.size; ++i) if (chosen[i] == name) duplicate = true;
            if (duplicate) continue;
            chosen[g.reward_cards.size] = name;
            g.reward_cards.push_back(create_card(g, make_card(name)));
        }
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.payload.state.ChooseOneColorless.opened) {
            generate_card_choices(g);
            g.discovery_card = -1;
            g.reward_skippable = false;
            e.payload.state.ChooseOneColorless.opened = true;
            g.screen = Screen::CardReward;
            return;
        }
        if (!e.payload.state.ChooseOneColorless.retrieve_card) {
            if (g.discovery_card >= 0) {
                Card copy = card_makeStatEquivalentCopy(g, g.discovery_card);
                if (find_power(g, g.player.creature, PowerId::MasterReality) >= 0) card_upgrade(copy);
                copy.uuid = g.next_card_uuid++;
                const int card = g.cards.add(copy);
                // ChooseOneColorless.update creates one copy even when amount is greater than one.
                if (e.amount == 1 ? g.hand.size < hand_limit :
                    g.hand.size + e.amount <= hand_limit || g.hand.size == 9) {
                    const CardType type = card_def(copy.id).type;
                    if (type != CardType::Curse && type != CardType::Status &&
                        find_power(g, g.player.creature, PowerId::MasterReality) >= 0) card_upgrade(g.cards[card]);
                    g.hand.push_back(card);
                    card_triggerWhenCopied(g, card);
                    refresh_hand(g);
                    for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
                    on_draw_or_discard(g);
                    if (find_power(g, g.player.creature, PowerId::Corruption) >= 0 && type == CardType::Skill)
                        g.cards[card].cost_for_turn = -9;
                } else {
                    g.discard.push_back(card);
                }
                g.discovery_card = -1;
            }
            e.payload.state.ChooseOneColorless.retrieve_card = true;
        }
    }
};
}
