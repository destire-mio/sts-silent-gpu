#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct FrozenEgg2 : RelicHooks {
    static SPIRE_FN void onEquip(Game& g, int relic) {
        for (int i = 0; i < g.reward_cards.size; ++i) onPreviewObtainCard(g, relic, g.cards[g.reward_cards[i]]);
    }
    static SPIRE_FN void onPreviewObtainCard(Game& g, int relic, Card& card) { onObtainCard(g, relic, card); }
    static SPIRE_FN void onObtainCard(Game&, int, Card& card) {
        if (card_data(card.id).type == CardType::Power && card.upgrades == 0) card_upgrade(card);
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
};
}
