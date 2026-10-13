#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct BustedCrown : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void onEquip(Game& g, int) { ++g.player.energy_master; }
    static SPIRE_FN void onUnequip(Game& g, int) { --g.player.energy_master; }
    static SPIRE_FN int changeNumberOfCardsInReward(Game&, int, int amount) { return amount - 2; }
};
}
