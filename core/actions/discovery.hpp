#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Discovery : ActionHooks {
    SPIRE_SNAPSHOT("DiscoveryAction.cardType", card_type, effect.state.Discovery.cardType)
    SPIRE_STATE(Discovery, bool retrieveCard; bool returnColorless; int cardType; bool selecting;)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_FN static Effect construct() {
        Effect e{};
        e.kind = EffectKind::Discovery;
        e.amount = 1;
        return e;
    }
    SPIRE_FN static Effect construct(CardType type, int amount) {
        Effect e = construct();
        e.amount = amount;
        e.extra = static_cast<int>(type) + 1;
        return e;
    }
    SPIRE_FN static Effect construct(bool colorless, int amount) {
        Effect e = construct();
        e.amount = amount;
        e.flag = colorless;
        return e;
    }
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.state.Discovery.returnColorless = e.flag;
        e.state.Discovery.cardType = e.extra - 1;
    }
    SPIRE_FN static void generate(Game& g, const Effect& e, const char* (&choices)[3]) {
        int count = 0;
        while (count != 3) {
            const char* name = random_card_in_combat(g, e.state.Discovery.cardType, e.state.Discovery.returnColorless);
            bool duplicate = false;
            for (int i = 0; i < count; ++i) if (same_id(choices[i], name)) duplicate = true;
            if (!duplicate) choices[count++] = name;
        }
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        const char* choices[3]{};
        generate(g, e, choices);
        if (!e.state.Discovery.selecting) {
            g.reward_cards.clear();
            for (int i = 0; i < 3; ++i)
                g.reward_cards.push_back(create_card(g, make_card(card_id(choices[i]))));
            g.discovery_card = -1;
            g.reward_skippable = e.state.Discovery.cardType >= 0;
            g.screen = Screen::CardReward;
            e.state.Discovery.selecting = true;
            return;
        }
        if (!e.state.Discovery.retrieveCard) {
            if (g.discovery_card >= 0) {
                Card first = card_makeStatEquivalentCopy(g, g.discovery_card);
                Card second = card_makeStatEquivalentCopy(g, g.discovery_card);
                if (has_power(g.player.creature, "MasterRealityPower")) {
                    card_upgrade(first);
                    card_upgrade(second);
                }
                if (first.cost_for_turn >= 0) first.cost_for_turn = 0;
                if (second.cost_for_turn >= 0) second.cost_for_turn = 0;
                const int first_index = create_card(g, first);
                if (e.amount == 1) {
                    if (g.hand.size < hand_limit) add_created_card_to_hand(g, first_index);
                    else g.discard.push_back(first_index);
                } else {
                    const int second_index = create_card(g, second);
                    if (g.hand.size + e.amount <= hand_limit) {
                        add_created_card_to_hand(g, first_index);
                        add_created_card_to_hand(g, second_index);
                    } else if (g.hand.size == 9) {
                        add_created_card_to_hand(g, first_index);
                        g.discard.push_back(second_index);
                    } else {
                        g.discard.push_back(first_index);
                        g.discard.push_back(second_index);
                    }
                }
                g.discovery_card = -1;
            }
            e.state.Discovery.retrieveCard = true;
        }
        // DiscoveryAction.update generates choices on all 16 ticks of ACTION_DUR_FAST at 1/60.
        for (int i = 0; i < 14; ++i) generate(g, e, choices);
    }
};
}
