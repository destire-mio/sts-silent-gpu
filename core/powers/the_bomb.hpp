#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::powers {
struct TheBomb : PowerHooks {
    SPIRE_STATIC("bombIdOffset", game.bomb_id_offset)
    SPIRE_STATE(TheBomb, int damage;)
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void constructPower(Game& g, Power& p, int, int, int damage, int) {
        power_initialize_state(p);
        p.state.TheBomb.damage = damage;
        // TheBombPower constructor assigns a new ID for every bomb, across combats.
        p.id_offset = g.bomb_id_offset++;
    }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int owner, bool) {
        if (!monsters_basically_dead(g)) {
            Effect e{};
            e.kind = EffectKind::ReducePower;
            e.source = owner;
            e.target = owner;
            e.power = PowerId::TheBomb;
            e.extra = p.instance;
            e.amount = 1;
            add_bottom(g, e);
            if (p.amount == 1) {
                int amounts[monster_capacity];
            for (int i = 0; i < g.monster_order.size; ++i) amounts[i] = p.state.TheBomb.damage;
            add_bottom(g, actions::DamageAllEnemies::make(g, no_target, amounts, g.monster_order.size, DamageType::Thorns));
            }
        }
    }
};
}
