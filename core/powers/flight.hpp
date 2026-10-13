#pragma once
#include "../hooks.hpp"
#include "../actions/change_state.hpp"

namespace spire::units::powers {
struct Flight : PowerHooks {
    SPIRE_STATE(Flight, int storedAmount;)
    static SPIRE_FN constexpr PowerData data() { return {50}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) {
        p.state.Flight.storedAmount = p.amount;
    }
    static SPIRE_FN void atStartOfTurn(Game&, Power& p, int) {
        p.amount = p.state.Flight.storedAmount;
    }
    static SPIRE_FN float atDamageFinalReceive(const Game&, const Power&, int, float value, DamageType type, int = -1) {
        return type == DamageType::Normal ? value / 2.0f : value;
    }
    static SPIRE_FN int onAttacked(Game& g, Power&, int owner, const DamageInfo& info, int amount) {
        // FlightPower.onAttacked halves the post-block damage again for willLive.
        const float taken = info.damage_type == DamageType::Normal ? static_cast<float>(amount) / 2.0f : static_cast<float>(amount);
        if (info.source != no_target && info.damage_type == DamageType::Normal && amount > 0 &&
            taken < static_cast<float>(get_creature(g, owner).hp)) {
            Effect e{};
            e.kind = EffectKind::ReducePower;
            e.source = owner;
            e.target = owner;
            e.power = PowerId::Flight;
            e.amount = 1;
            add_bottom(g, e);
        }
        return amount;
    }
    static SPIRE_FN void onRemove(Game& g, Power&, int owner) {
        add_bottom(g, actions::ChangeState::make(g, owner, "GROUNDED"));
    }
};
}
