#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct BeatOfDeath : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onAfterUseCard(Game& g, Power& p, int owner, int, Effect&) {
        Effect e{};
        e.kind = EffectKind::Damage;
        e.source = owner;
        e.target = player_target;
        e.amount = p.amount;
        e.info_base = p.amount;
        e.damage_type = DamageType::Thorns;
        add_bottom(g, e);
    }
};
}
