#pragma once
#include "../hooks.hpp"
#include "../actions/change_state.hpp"

namespace spire::units::powers {
struct CurlUp : PowerHooks {
    SPIRE_SNAPSHOT("CurlUpPower.triggered", scalar, power.triggered)
    static SPIRE_FN constexpr PowerData data() { return {5, false, false}; }
    static SPIRE_FN int onAttacked(Game& g, Power& p, int owner, const DamageInfo& info, int amount) {
        if (!p.triggered && amount < get_creature(g, owner).hp && amount > 0 &&
            info.source != no_target && info.damage_type == DamageType::Normal) {
            p.triggered = true;
            add_bottom(g, actions::ChangeState::make(g, owner, "CLOSED"));
            queue_block(g, owner, p.amount);
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.target = owner;
            e.source = owner;
            e.power = PowerId::CurlUp;
            add_bottom(g, e);
        }
        return amount;
    }
};
}
