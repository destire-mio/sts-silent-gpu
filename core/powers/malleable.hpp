#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Malleable : PowerHooks {
    SPIRE_STATE(Malleable, int basePower;)
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) {
        p.state.Malleable.basePower = p.amount;
    }
    static SPIRE_FN void atEndOfTurn(Game&, Power& p, int owner, bool) {
        if (owner == player_target) return;
        p.amount = p.state.Malleable.basePower;
    }
    static SPIRE_FN void atEndOfRound(Game&, Power& p, int owner) {
        if (owner != player_target) return;
        p.amount = p.state.Malleable.basePower;
    }
    static SPIRE_FN int onAttacked(Game& g, Power& p, int owner, const DamageInfo& info, int amount) {
        if (amount < get_creature(g, owner).hp && amount > 0 &&
            info.source != no_target && info.damage_type == DamageType::Normal) {
            Effect e{};
            e.kind = EffectKind::Block;
            e.source = owner;
            e.target = owner;
            e.amount = p.amount;
            if (owner == player_target) add_top(g, e);
            else add_bottom(g, e);
            ++p.amount;
        }
        return amount;
    }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) {
        p.amount += amount;
        p.state.Malleable.basePower += amount;
    }
};
}
