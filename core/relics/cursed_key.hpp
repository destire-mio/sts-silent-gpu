#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct CursedKey : RelicHooks {
    static SPIRE_FN void justEnteredRoom(Game&, int, int) {}
    static SPIRE_FN void onChestOpen(Game& g, int relic, bool boss_chest) {
        if (!boss_chest) {
            // CardLibrary.getCurse iterates its curse-only HashMap, not curseCardPool.
            constexpr const char* curses[] = {"Regret", "Injury", "Shame", "Parasite", "Normality", "Doubt", "Writhe", "Pain", "Decay", "Clumsy"};
            queue_card_obtain(g, make_card(card_id(curses[g.rng.card.random(0, 9)])));
            queue_relic_above(g, relic, player_target, true);
        }
    }
    static SPIRE_FN void onEquip(Game& g, int) { ++g.player.energy_master; }
    static SPIRE_FN void onUnequip(Game& g, int) { --g.player.energy_master; }
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
};
}
