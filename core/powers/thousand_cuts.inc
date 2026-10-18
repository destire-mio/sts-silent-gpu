#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage_all_enemies.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::powers {
struct ThousandCuts : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) { p.amount += amount; }
    static SPIRE_FN void onAfterCardPlayed(Game& g, Power& p, int owner, int) {
        Effect sfx{};
        sfx.kind = EffectKind::SFX;
        add_bottom(g, sfx);
        add_bottom(g, actions::VFX::make(owner));
        int amounts[monster_capacity];
        for (int i = 0; i < g.monster_order.size; ++i) amounts[i] = p.amount;
        add_bottom(g, actions::DamageAllEnemies::make(g, owner, amounts, g.monster_order.size, DamageType::Thorns));
    }
};
}
