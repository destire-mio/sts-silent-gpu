#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::powers {
struct Combust : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    SPIRE_STATE(Combust, int hpLoss;)
    static SPIRE_FN void constructPower(Game &, Power& p, int, int, int extra, int) { p.state.Combust.hpLoss = extra; }
    static SPIRE_FN void stackPower(Game &, Power& p, int, int amount) {
        p.amount += amount;
        ++p.state.Combust.hpLoss;
    }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int owner, bool) {
        if (monsters_basically_dead(g))
            return;
        Effect e{};
        e.kind = EffectKind::LoseHP;
        e.source = e.target = owner;
        e.amount = p.state.Combust.hpLoss;
        add_bottom(g, e);
        int amounts[monster_capacity]{};
        for (int i = 0; i < g.monster_order.size; ++i)
            amounts[i] = p.amount;
        add_bottom(
            g, units::actions::DamageAllEnemies::make(g, no_target, amounts, g.monster_order.size, DamageType::Thorns));
    }
};
}
