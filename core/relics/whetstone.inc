#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct Whetstone : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 150}; d.screenless_excluded = true; return d; }
    static SPIRE_FN void onEquip(Game& g, int) {
        Pile upgradeable{};
        for (int i = 0; i < g.run.master_deck.size; ++i) {
            const Card& card = g.run.master_deck[i];
            if (card_canUpgrade(g, -i - 2) && card_data(card.id).type == CardType::Attack) upgradeable.push_back(i);
        }
        shuffle(upgradeable.values, upgradeable.size, g.rng.misc);
        for (int i = 0; i < min(2, static_cast<int>(upgradeable.size)); ++i)
            card_upgrade(g.run.master_deck[upgradeable[i]]);
    }
};
}
