#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct RunicDome : RelicHooks {
    SPIRE_NOT_APPLICABLE("hides_intents: AbstractMonster.render and renderTip only; no rule or included snapshot field reads it")
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void onEquip(Game& g, int) { ++g.player.energy_master; }
    static SPIRE_FN void onUnequip(Game& g, int) { --g.player.energy_master; }
};
}
