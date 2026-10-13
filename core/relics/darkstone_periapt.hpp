#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct DarkstonePeriapt : RelicHooks {
    static SPIRE_FN void onObtainCard(Game& g, int, Card& card) {
        if (card_data(card.id).type == CardType::Curse) increase_max_hp(g, 6);
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
};
}
