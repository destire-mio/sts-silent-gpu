#pragma once
#include "../hooks.hpp"
#include "show_card.hpp"
#include "apply_power.hpp"

namespace spire::units::actions {
struct ApplyStasisAction : ActionHooks {
    SPIRE_SNAPSHOT("ApplyStasisAction.owner", creature, effect.state.ApplyStasisAction.owner)
    SPIRE_SNAPSHOT("ApplyStasisAction.card", card, effect.state.ApplyStasisAction.card)
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_STATE(ApplyStasisAction, int owner; int card;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.state.ApplyStasisAction.owner = e.target;
        e.state.ApplyStasisAction.card = -1;
        e.source = e.target = no_target;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (g.draw.empty() && g.discard.empty()) { e.done = true; return; }
        Pile& pile = g.draw.empty() ? g.discard : g.draw;
        Pile candidates{};
        constexpr CardRarity rarities[]{CardRarity::Rare, CardRarity::Uncommon, CardRarity::Common};
        for (int rarity = 0; rarity < 3; ++rarity) {
            for (int i = 0; i < pile.size; ++i)
                if (card_rarity(g.cards[pile[i]].id) == rarities[rarity]) candidates.push_back(pile[i]);
            if (!candidates.empty()) break;
        }
        int card;
        if (candidates.empty()) card = pile[g.rng.card_random.random(pile.size - 1)];
        else {
            for (int i = 1; i < candidates.size; ++i) {
                const int value = candidates[i];
                int j = i;
                while (j > 0 && card_sort_order(g.cards[candidates[j - 1]].id) > card_sort_order(g.cards[value].id)) {
                    candidates[j] = candidates[j - 1];
                    --j;
                }
                candidates[j] = value;
            }
            card = candidates[g.rng.card_random.random(candidates.size - 1)];
        }
        for (int i = 0; i < pile.size; ++i) if (pile[i] == card) { pile.erase(i); break; }
        e.state.ApplyStasisAction.card = card;
        g.limbo.insert(0, card);
        Power power{PowerId::Stasis, -1};
        const int owner = e.state.ApplyStasisAction.owner;
        power_constructPower(g, power, owner, owner, 0, card);
        add_top(g, ApplyPower::make(owner, owner, power, 1));
        add_top(g, ShowCard::construct(card));
        e.done = true;
    }
};
}
