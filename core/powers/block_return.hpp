#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct BlockReturn : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
    static SPIRE_FN int onAttacked(Game& g, Power& p, int owner, const DamageInfo &info, int amount) {
        if (info.damage_type == DamageType::Normal && info.source != no_target && info.source != owner) {
            Effect e{};
            e.kind = EffectKind::Block;
            e.source = no_target;
            e.target = player_target;
            e.amount = p.amount;
            add_top(g, e);
        }
        return amount;
    }
    static SPIRE_FN void stackPower(Game &, Power& p, int, int amount) { p.amount += amount; }
};
}
