#pragma once
#include "../hooks.hpp"
#include "../actions/channel.hpp"

namespace spire::units::powers {
struct StaticDischarge : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN int onAttacked(Game& g, Power& p, int owner, const DamageInfo &info, int amount) {
        if (info.damage_type == DamageType::Normal && info.source != no_target && info.source != owner && amount > 0)
            for (int i = 0; i < p.amount; ++i)
                add_top(g, units::actions::Channel::make(make_orb(g, OrbKind::Lightning)));
        return amount;
    }
};
}
