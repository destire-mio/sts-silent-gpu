#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct TimeWarp : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = 0; }
    static SPIRE_FN void onAfterUseCard(Game& g, Power& p, int, int, Effect&) {
        ++p.amount;
        if (p.amount == 12) {
            p.amount = 0;
            for (int i = 0; i < g.monster_order.size; ++i) {
                const int owner = g.monster_order[i];
                Effect e{};
                e.kind = EffectKind::ApplyPower;
                e.source = owner;
                e.target = owner;
                e.power = PowerId::Strength;
                e.amount = 2;
                add_bottom(g, e);
            }
        }
    }
};
}
