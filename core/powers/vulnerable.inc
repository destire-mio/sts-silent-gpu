#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Vulnerable : PowerHooks {
    SPIRE_SNAPSHOT("VulnerablePower.justApplied", scalar, power.just_applied)
    static SPIRE_FN constexpr PowerData data() { PowerData data{5, true, false}; data.turn_based = true; return data; }
    static SPIRE_FN void initFromSource(Game& g, Power& p, int, bool monster_source) {
        p.just_applied = monster_source && g.manager.turn_has_ended;
    }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        if (p.just_applied) { p.just_applied = false; return; }
        Effect e{};
        e.kind = p.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.target = owner;
        e.source = owner;
        e.power = PowerId::Vulnerable;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN float atDamageReceive(const Game& g, const Power&, int owner, float value, DamageType type, int = -1) {
        if (type != DamageType::Normal) return value;
        if (owner == player_target) {
            for (int i = 0; i < g.run.relics.size; ++i) {
                const float multiplier = relic_data(g.run.relics[i].id).player_vulnerable_multiplier;
                if (multiplier != 0.0f) return value * multiplier;
            }
        }
        return value * 1.5f;
    }
};
}
