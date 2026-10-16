#pragma once
#include "../hooks.hpp"
#include "damage_all_enemies.hpp"

namespace spire::units::actions {
struct Whirlwind : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_STATE(Whirlwind, int energyOnUse; bool freeToPlayOnce;)
    SPIRE_SNAPSHOT("WhirlwindAction.multiDamage", damage_array, game.multi_damage.get(effect.card), game.cards[effect.card].multi_damage_count, effect.card, true)
    SPIRE_SNAPSHOT("WhirlwindAction.damageType", damage_type, effect.damage_type)
    SPIRE_SNAPSHOT("WhirlwindAction.p", player)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        int count = e.payload.state.Whirlwind.energyOnUse < 0 ? g.player.energy : e.payload.state.Whirlwind.energyOnUse;
        if (has_relic(g, RelicId::ChemicalX))
            count += 2;
        if (count > 0) {
            for (int i = 0; i < count; ++i) {
                Effect v{};
                v.source = v.target = no_target;
                if (i == 0) {
                    v.kind = EffectKind::SFX;
                    add_bottom(g, v);
                    v.kind = EffectKind::VFX;
                    add_bottom(g, v);
                }
                v.kind = EffectKind::SFX;
                add_bottom(g, v);
                v.kind = EffectKind::VFX;
                v.source = v.target = player_target;
                add_bottom(g, v);
                add_bottom(g, units::actions::DamageAllEnemies::from_card(g, e.card, e.damage_type));
            }
            if (!e.payload.state.Whirlwind.freeToPlayOnce)
                g.player.energy = 0;
        }
    }
};
}
