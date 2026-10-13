#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct TheBomb : PowerHooks {
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
            e.power_offset = p.id_offset;
            e.amount = 1;
            add_bottom(g, e);
        }
    }
};
}
