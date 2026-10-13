#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct LoseDexterity : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true, false}; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int owner, bool) {
        Effect dexterity{};
        dexterity.kind = EffectKind::ApplyPower;
        dexterity.source = owner;
        dexterity.target = owner;
        dexterity.power = PowerId::Dexterity;
        dexterity.amount = -p.amount;
        add_bottom(g, dexterity);
        Effect remove{};
        remove.kind = EffectKind::RemovePower;
        remove.source = owner;
        remove.target = owner;
        remove.power = PowerId::LoseDexterity;
        add_bottom(g, remove);
    }
};
}
