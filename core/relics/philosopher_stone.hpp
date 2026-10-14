#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct PhilosopherStone : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void add_strength(Game& g, int owner) {
        Powers& powers = g.monsters[owner].creature.powers;
        bool stacked = false;
        for (int i = 0; i < powers.size; ++i) {
            Power& p = powers.at(g.power_objects, i);
            if (p.id != PowerId::Strength) continue;
            power_stackPower(g, p, owner, 1);
            stacked = true;
        }
        if (!stacked) {
            Power p(PowerId::Strength, 1);
            p.instance = next_power_identity(g);
            power_constructPower(g, p, owner, owner, 0, -1);
            powers.push_back(g.power_objects, g.power_objects.add(p));
        }
    }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int owner = g.monster_order[i];
            queue_relic_above(g, relic, owner, true);
            add_strength(g, owner);
        }
        refresh_powers(g);
    }
    static SPIRE_FN void onSpawnMonster(Game& g, int, int owner) {
        add_strength(g, owner);
        refresh_powers(g);
    }
    static SPIRE_FN void onEquip(Game& g, int) { ++g.player.energy_master; }
    static SPIRE_FN void onUnequip(Game& g, int) { --g.player.energy_master; }
};
}
