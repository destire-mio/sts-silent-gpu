#pragma once
#include "core/api.hpp"

namespace spire {
struct PowerReferences {
    uint64_t words[(power_object_capacity + 63) / 64]{};
    uint64_t monsters = 0;
    SPIRE_FN void creature(const Game& g, int owner) {
        if (owner < 0) return;
        SPIRE_ASSERT(owner < g.monsters.size);
        monsters |= uint64_t{1} << owner;
    }
    SPIRE_FN void power(const Game& g, const Power& value) {
        if (value.id == PowerId::Poison) creature(g, value.state.Poison.source);
        if (value.id == PowerId::Constricted) creature(g, value.state.Constricted.source);
        if (value.id == PowerId::Minion) creature(g, value.state.Minion.owner);
    }
    SPIRE_FN void add(const Game& g, int index) {
        SPIRE_ASSERT(index >= 0 && index < power_object_capacity && g.power_objects.contains(index));
        words[index / 64] |= uint64_t{1} << (index % 64);
        power(g, g.power_objects[index]);
    }
    SPIRE_FN void powers(const Game& g, const Powers& values) {
        int index = values.head;
        for (int i = 0; i < values.size; ++i) {
            add(g, index);
            index = g.power_objects[index].next;
        }
        SPIRE_ASSERT(index == 255);
    }
};

SPIRE_FN inline PowerReferences power_references(const Game& g) {
    PowerReferences refs{};
    refs.powers(g, g.player.creature.powers);
    for (int i = 0; i < g.monster_order.size; ++i) refs.creature(g, g.monster_order[i]);
    for (int i = 0; i < g.manager.monster_queue.size; ++i) refs.creature(g, g.manager.monster_queue[i]);
    for (int i = 0; i < g.manager.card_queue.size; ++i) refs.creature(g, g.manager.card_queue[i].target);
    for (int word = 0; word < (action_capacity + 63) / 64; ++word) {
        uint64_t used = g.action_objects.used[word];
        while (used) {
            const int i = word * 64 + __builtin_ctzll(used);
            used &= used - 1;
            const Effect& e = g.action_objects[i];
            refs.creature(g, e.source); refs.creature(g, e.target);
            // ReducePowerAction.update can modify a detached power through powerInstance.
            if ((e.kind == EffectKind::RemovePower || e.kind == EffectKind::ReducePower || e.kind == EffectKind::MayhemPlay) && e.extra != 0) {
                const Power& power = power_instance(g, e.extra);
                refs.add(g, static_cast<int>(&power - g.power_objects.values));
            }
            if ((e.kind == EffectKind::ApplyPower && e.power_constructed) || e.kind == EffectKind::ApplyPowerToRandomEnemy)
                refs.power(g, e.payload.power);
            switch (e.kind) {
            case EffectKind::SpawnMonster: refs.creature(g, e.payload.state.SpawnMonster.m); break;
            case EffectKind::ApplyStasisAction: refs.creature(g, e.payload.state.ApplyStasisAction.owner); break;
            case EffectKind::ChangeState: refs.creature(g, e.payload.state.ChangeState.m); break;
            case EffectKind::SummonGremlinAction: refs.creature(g, e.payload.state.SummonGremlinAction.m); break;
            case EffectKind::MalaiseAction:
                refs.creature(g, e.payload.state.MalaiseAction.p); refs.creature(g, e.payload.state.MalaiseAction.m); break;
            case EffectKind::SkewerAction:
                refs.creature(g, e.payload.state.SkewerAction.p); refs.creature(g, e.payload.state.SkewerAction.m); break;
            case EffectKind::TransmutationAction: refs.creature(g, e.payload.state.TransmutationAction.p); break;
            case EffectKind::Barrage: case EffectKind::FTL: case EffectKind::ExpungeVFX:
            case EffectKind::FearNoEvil: case EffectKind::ForTheEyes: case EffectKind::IntentFlash:
                refs.creature(g, e.extra); break;
            default: break;
            }
        }
    }
    uint64_t visited = 0;
    while (refs.monsters & ~visited) {
        const int index = __builtin_ctzll(refs.monsters & ~visited);
        visited |= uint64_t{1} << index;
        const Monster& m = g.monsters[index];
        refs.powers(g, m.creature.powers);
        if (m.id == MonsterId::GremlinLeader)
            for (int owner : m.state.GremlinLeader.gremlins) refs.creature(g, owner);
        if (m.id == MonsterId::Reptomancer)
            for (int owner : m.state.Reptomancer.daggers) refs.creature(g, owner);
        if (m.id == MonsterId::TheCollector)
            for (int owner : m.state.TheCollector.enemySlots) refs.creature(g, owner);
    }
    return refs;
}

SPIRE_FN inline void collect_powers(Game& g) {
    const PowerReferences refs = power_references(g);
    for (int word = 0; word < (power_object_capacity + 63) / 64; ++word) {
        SPIRE_ASSERT((refs.words[word] & ~g.power_objects.used[word]) == 0);
        uint64_t garbage = g.power_objects.used[word] & ~refs.words[word];
        while (garbage) {
            const int index = word * 64 + __builtin_ctzll(garbage);
            g.power_objects.release(index);
            garbage &= garbage - 1;
        }
    }
}
}
