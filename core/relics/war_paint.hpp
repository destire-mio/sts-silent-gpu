#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct WarPaint : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onEquip(Game& g, int) {
        Pile upgradeable{};
        for (int i = 0; i < g.run.master_deck.size; ++i) {
            const Card& card = g.run.master_deck[i];
            if (card_can_upgrade(card) && card_data(card.id).type == CardType::Skill) upgradeable.push_back(i);
        }
        shuffle(upgradeable.values, upgradeable.size, g.rng.misc);
        for (int i = 0; i < min(2, static_cast<int>(upgradeable.size)); ++i)
            card_upgrade(g.run.master_deck[upgradeable[i]]);
    }
};
}
