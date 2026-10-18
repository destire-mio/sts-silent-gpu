#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct EndTurnDeath : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game &, Power& p, int) { p.amount = -1; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power &, int owner) {
        Effect vfx{};
        vfx.kind = EffectKind::VFX;
        vfx.source = vfx.target = no_target;
        add_bottom(g, vfx);
        Effect e{};
        e.kind = EffectKind::LoseHP;
        e.source = e.target = owner;
        e.amount = 99999;
        add_bottom(g, e);
        {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = e.target = owner;
            e.power = PowerId::EndTurnDeath;
            add_bottom(g, e);
        }
    }
};
}
