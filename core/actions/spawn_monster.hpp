#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct SpawnMonster : ActionHooks {
    SPIRE_SNAPSHOT("SpawnMonsterAction.m", monster, effect.payload.state.SpawnMonster.m)
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_STATE(SpawnMonster, int8_t m; bool minion; int8_t target_slot; bool use_smart_positioning; bool used;)
    SPIRE_FN static Effect construct(int monster, bool minion) {
        Effect e = construct(monster, minion, -99);
        e.payload.state.SpawnMonster.use_smart_positioning = true;
        return e;
    }
    SPIRE_FN static Effect construct(int monster, bool minion, int slot) {
        Effect e{};
        e.kind = EffectKind::SpawnMonster;
        action_initialize_state(e);
        e.source = no_target;
        e.target = no_target;
        e.payload.state.SpawnMonster.m = monster;
        e.payload.state.SpawnMonster.minion = minion;
        e.payload.state.SpawnMonster.target_slot = slot;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        auto& state = e.payload.state.SpawnMonster;
        if (state.used) return;
        for (int i = 0; i < g.monsters.size; ++i)
            if (g.monsters[i].creature.dying) g.deaths_before_monster_turn |= uint64_t{1} << i;
        for (int i = 0; i < g.run.relics.size; ++i) relic_onSpawnMonster(g, i, state.m);
        monster_init(g, state.m);
        monster_applyPowers(g, state.m);
        int position = state.target_slot;
        if (state.use_smart_positioning) {
            position = 0;
            for (int i = 0; i < g.monster_order.size; ++i)
                if (g.monsters[state.m].creature.draw_x > g.monsters[g.monster_order[i]].creature.draw_x) ++position;
        }
        g.monster_order.insert(position, state.m);
        if (state.minion) {
            Effect apply{};
            apply.kind = EffectKind::ApplyPower;
            apply.source = state.m;
            apply.target = state.m;
            apply.power = power_id("Minion");
            apply.amount = -1;
            add_top(g, apply);
        }
        state.used = true;
    }
};
}
