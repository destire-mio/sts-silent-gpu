#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct SummonGremlinAction : ActionHooks {
    SPIRE_STATE(SummonGremlinAction, int m;)
    SPIRE_FN static Effect construct(Game& g, int (&gremlins)[3]) {
        Effect e{};
        e.kind = EffectKind::SummonGremlinAction;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.state.SummonGremlinAction.m = no_target;
        int slot = -1;
        for (int i = 0; i < 3; ++i) {
            if (gremlins[i] >= 0 && !g.monsters[gremlins[i]].creature.dying) continue;
            slot = i;
            break;
        }
        if (slot == -1) return e;
        constexpr MonsterId pool[]{MonsterId::GremlinWarrior, MonsterId::GremlinWarrior,
            MonsterId::GremlinThief, MonsterId::GremlinThief, MonsterId::GremlinFat,
            MonsterId::GremlinFat, MonsterId::GremlinTsundere, MonsterId::GremlinWizard};
        constexpr float positions[]{-366.0f, -170.0f, -532.0f};
        const MonsterId id = pool[g.rng.ai.random(0, 7)];
        Monster monster = construct_monster(g, id);
        monster.offset_x = positions[slot];
        monster.creature.draw_x = 1280.0f * 0.75f + positions[slot] * (1280.0f / 1920.0f);
        const int owner = g.monsters.size;
        g.monsters.push_back(monster);
        gremlins[slot] = e.state.SummonGremlinAction.m = owner;
        for (int i = 0; i < g.run.relics.size; ++i) relic_onSpawnMonster(g, i, owner);
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        const int owner = e.state.SummonGremlinAction.m;
        SPIRE_ASSERT(owner != no_target);
        monster_init(g, owner);
        monster_applyPowers(g, owner);
        int position = 0;
        for (int i = 0; i < g.monster_order.size; ++i) {
            if (!(g.monsters[owner].creature.draw_x > g.monsters[g.monster_order[i]].creature.draw_x)) break;
            ++position;
        }
        g.monster_order.insert(position, owner);
        monster_usePreBattleAction(g, owner);
        e.done = true;
    }
};
}
