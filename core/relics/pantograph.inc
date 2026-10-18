#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct Pantograph : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        for (int i = 0; i < g.monster_order.size; ++i) {
            if (monster_data(g.monsters[g.monster_order[i]].id).type != EnemyType::Boss) continue;
            Effect e{};
            e.kind = EffectKind::Heal;
            e.amount = 25;
            add_top(g, e);
            queue_relic_above(g, relic, player_target, true);
            return;
        }
    }
};
}
