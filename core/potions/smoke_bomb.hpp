#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct SmokeBomb : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {0, CardTarget::Self, true}; }
    static SPIRE_FN bool canUse(const Game& g, int slot) {
        if (!potion_can_use_base(g, slot)) return false;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const Monster& monster = g.monsters[g.monster_order[i]];
            if (has_power(g, monster.creature, PowerId::BackAttack) || monster_data(monster.id).type == EnemyType::Boss)
                return false;
        }
        return true;
    }
    static SPIRE_FN void use(Game& g, int, int) {
        if (g.room_in_combat) {
            g.smoked = true;
            add_bottom(g, units::actions::VFX::make());
            g.player.escaping = true;
            g.player.creature.flip_horizontal = !g.player.creature.flip_horizontal;
            g.end_turn_enabled = false;
            g.player.escape_timer = 2.5f;
        }
    }
};
}
