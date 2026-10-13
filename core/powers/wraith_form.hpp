#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct WraithForm : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true, true}; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int, bool) {
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        e.source = player_target;
        e.target = player_target;
        e.power = PowerId::Dexterity;
        e.amount = p.amount;
        add_bottom(g, e);
    }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) { p.amount += amount; }
};
}
