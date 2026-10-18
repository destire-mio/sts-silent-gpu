#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct ToyOrnithopter : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onUsePotion(Game& g, int relic) {
        if (g.room_in_combat) {
        Effect show{};
        show.kind = EffectKind::RelicAboveCreature;
        show.extra = relic;
        add_bottom(g, show);
            Effect heal{};
            heal.kind = EffectKind::Heal;
            heal.amount = 5;
            add_bottom(g, heal);
        } else heal_creature(g, player_target, 5);
    }
};
}
