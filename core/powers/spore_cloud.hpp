#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct SporeCloud : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, false, false}; }
    static SPIRE_FN void onDeath(Game& g, Power& p, int) {
        if (g.battle_over || monsters_basically_dead(g)) return;
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        e.source = no_target;
        e.target = player_target;
        e.power = PowerId::Vulnerable;
        e.amount = p.amount;
        e.flag = g.manager.turn_has_ended;
        add_top(g, e);
    }
};
}
