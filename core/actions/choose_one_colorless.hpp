#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ChooseOneColorless : ActionHooks {
    SPIRE_STATE(ChooseOneColorless, bool retrieve_card; bool opened;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.source = no_target;
        e.target = no_target;
        e.amount = 1;
    }
    SPIRE_FN static void generate_card_choices(Game& g) {
        // AbstractDungeon.initializeCardPools reverses colorlessCardPool into its source pool.
        constexpr const char* choices[] = {
            "Madness",
            "Thinking Ahead",
            "Mind Blast",
            "Metamorphosis",
            "Jack Of All Trades",
            "Swift Strike",
            "Good Instincts",
            "Master of Strategy",
            "Magnetism",
            "Finesse",
            "Discovery",
            "Chrysalis",
            "Transmutation",
            "Panacea",
            "Purity",
            "Enlightenment",
            "Forethought",
            "Flash of Steel",
            "HandOfGreed",
            "Mayhem",
            "Apotheosis",
            "Secret Weapon",
            "Panache",
            "Violence",
            "Deep Breath",
            "Secret Technique",
            "Blind",
            "The Bomb",
            "Impatience",
            "Dramatic Entrance",
            "Trip",
            "PanicButton",
            "Sadistic Nature",
            "Dark Shackles",
        };
        g.reward_cards.clear();
        int chosen[3]{};
        while (g.reward_cards.size != 3) {
            const int index = g.rng.card_random.random(static_cast<int>(sizeof(choices) / sizeof(choices[0])) - 1);
            bool duplicate = false;
            for (int i = 0; i < g.reward_cards.size; ++i) if (chosen[i] == index) duplicate = true;
            if (duplicate) continue;
            chosen[g.reward_cards.size] = index;
            const int card = g.cards.size;
            g.cards.push_back(make_card(card_id(choices[index])));
            g.cards[card].uuid = card + 1;
            g.reward_cards.push_back(card);
        }
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.state.ChooseOneColorless.opened) {
            generate_card_choices(g);
            g.discovery_card = -1;
            e.state.ChooseOneColorless.opened = true;
            g.screen = Screen::CardReward;
            return;
        }
        if (!e.state.ChooseOneColorless.retrieve_card) {
            if (g.discovery_card >= 0) {
                Card copy = card_makeStatEquivalentCopy(g, g.discovery_card);
                if (find_power(g.player.creature, "MasterRealityPower") >= 0) card_upgrade(copy);
                const int card = g.cards.size;
                copy.uuid = card + 1;
                g.cards.push_back(copy);
                // ChooseOneColorless.update creates one copy even when amount is greater than one.
                if (e.amount == 1 ? g.hand.size < hand_limit :
                    g.hand.size + e.amount <= hand_limit || g.hand.size == 9) {
                    const CardType type = card_def(copy.id).type;
                    if (type != CardType::Curse && type != CardType::Status &&
                        find_power(g.player.creature, "MasterRealityPower") >= 0) card_upgrade(g.cards[card]);
                    g.hand.push_back(card);
                    card_triggerWhenCopied(g, card);
                    refresh_hand(g);
                    for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
                    on_draw_or_discard(g);
                    if (find_power(g.player.creature, "Corruption") >= 0 && type == CardType::Skill)
                        g.cards[card].cost_for_turn = -9;
                } else {
                    g.discard.push_back(card);
                }
                g.discovery_card = -1;
            }
            e.state.ChooseOneColorless.retrieve_card = true;
        }
    }
};
}
