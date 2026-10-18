#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct UpgradeRandomCard : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("UpgradeRandomCardAction.p", player)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_FN static void execute(Game& g, const Effect&) {
        if (g.hand.empty()) return;
        Pile upgradeable{};
        for (int i = 0; i < g.hand.size; ++i) {
            const int card = g.hand[i];
            if (card_canUpgrade(g, card) && card_data(g.cards[card].id).type != CardType::Status)
                upgradeable.push_back(card);
        }
        if (!upgradeable.empty()) {
            shuffle(upgradeable.values, upgradeable.size, g.rng.shuffle);
            card_upgrade(g.cards[upgradeable[0]]);
            card_applyPowers(g, upgradeable[0]);
        }
    }
};
}
