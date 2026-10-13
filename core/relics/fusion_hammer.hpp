#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct FusionHammer : RelicHooks {
    static SPIRE_FN void onEquip(Game& g, int) { ++g.player.energy_master; }
    static SPIRE_FN void onUnequip(Game& g, int) { --g.player.energy_master; }
    static SPIRE_FN bool canUseCampfireOption(const Game&, int, int option) { return option != static_cast<int>(CampfireOption::Smith); }
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
};
}
