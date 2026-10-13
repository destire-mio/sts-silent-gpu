#pragma once
#include "../hooks.hpp"
#include "../actions/change_state.hpp"

namespace spire::units::powers {
struct PlatedArmor : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) { p.amount = min(999, p.amount + amount); }
    static SPIRE_FN void wasHPLost(Game& g, Power&, int owner, const DamageInfo& info, int amount) {
        if (info.source != no_target && info.source != owner && info.damage_type != DamageType::HpLoss &&
            info.damage_type != DamageType::Thorns && amount > 0) {
            Effect e{};
            e.kind = EffectKind::ReducePower;
            e.source = owner;
            e.target = owner;
            e.power = PowerId::PlatedArmor;
            e.amount = 1;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN void atEndOfTurnPreEndTurnCards(Game& g, Power& p, int owner, bool) {
        queue_block(g, owner, p.amount);
    }
    static SPIRE_FN void onRemove(Game& g, Power&, int owner) {
        if (owner != player_target) add_bottom(g, actions::ChangeState::make(owner, "ARMOR_BREAK"));
    }
};
}
