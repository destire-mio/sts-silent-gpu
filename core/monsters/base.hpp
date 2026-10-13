#pragma once
#include "../hooks.hpp"

namespace spire {
SPIRE_FN inline Monster make_monster(Game& g, MonsterId id, const MonsterData& data, int hp_override) {
    Monster m{};
    m.id = id;
    monster_initialize_state(m);
    m.creature.hp = hp_override == 0 ? g.rng.monster_hp.random(data.hp_min, data.hp_max) : hp_override;
    m.creature.max_hp = m.creature.hp;
    m.damage[0] = data.damage[0];
    m.damage[1] = data.damage[1];
    m.block_amount = data.block;
    m.magic = data.magic;
    return m;
}

SPIRE_FN inline void append_monster(Game& g, const Monster& monster) {
    g.monster_order.push_back(g.monsters.size);
    g.monsters.push_back(monster);
}

SPIRE_FN inline bool last_move(const Monster& m, int move) {
    return m.move_history.size > 0 && m.move_history[m.move_history.size - 1] == move;
}

SPIRE_FN inline bool last_two_moves(const Monster& m, int move) {
    return m.move_history.size >= 2 && m.move_history[m.move_history.size - 1] == move &&
        m.move_history[m.move_history.size - 2] == move;
}

SPIRE_FN inline void set_monster_move(Game& g, int index, int move, Intent intent, int base_damage, int hits) {
    Monster& m = g.monsters[index];
    m.pending_move = move;
    m.pending_intent = intent;
    m.pending_base_damage = base_damage;
    m.pending_hits = hits > 1 ? hits : -1;
    if (move != -1) m.move_history.push_back(move);
}

SPIRE_FN inline void create_monster_intent(Game& g, int index) {
    Monster& m = g.monsters[index];
    m.move = m.pending_move;
    m.intent = m.pending_intent;
    m.intent_base_damage = m.pending_base_damage;
    if (m.pending_base_damage >= 0) {
        m.intent_damage = calculate_damage(g, index, player_target, m.pending_base_damage);
        m.intent_hits = m.pending_hits;
    }
}

SPIRE_FN inline void queue_monster_move(Game& g, int index, int move, Intent intent, int damage = -1) {
    Effect e{};
    e.kind = EffectKind::SetMove;
    e.target = index;
    e.amount = move;
    e.extra = static_cast<int>(intent);
    e.card = damage;
    add_bottom(g, e);
}

SPIRE_FN inline void queue_monster_roll(Game& g, int index) {
    Effect e{};
    e.kind = EffectKind::RollMove;
    e.target = index;
    add_bottom(g, e);
}

SPIRE_FN inline void queue_slimed(Game& g, int amount) {
    Effect e{};
    e.kind = EffectKind::AddCardDiscard;
    e.card = static_cast<int>(CardId::Slimed);
    e.amount = amount;
    add_bottom(g, e);
}

SPIRE_FN inline void roll_monster_move(Game& g, int index) {
    monster_rollMove(g, index);
}

SPIRE_FN inline void interrupt_for_split(Game& g, int index) {
    Monster& m = g.monsters[index];
    if (!m.creature.dying && static_cast<float>(m.creature.hp) <= static_cast<float>(m.creature.max_hp) / 2.0f &&
        m.move != 3 && !m.split_triggered) {
        set_monster_move(g, index, 3, Intent::Unknown);
        monster_createIntent(g, index);
        queue_monster_move(g, index, 3, Intent::Unknown);
        m.split_triggered = true;
    }
}

SPIRE_FN inline void split_into(Game& g, int index, MonsterId child) {
    Monster& parent = g.monsters[index];
    const int hp = parent.creature.hp;
    parent.creature.hp = 0;
    monster_die(g, index, false);
    int position = 0;
    while (g.monster_order[position] != index) ++position;
    const int left = g.monsters.size;
    g.monsters.push_back(construct_monster(g, child, hp));
    for (int i = 0; i < g.run.relics.size; ++i) relic_onSpawnMonster(g, i, left);
    monster_init(g, left);
    monster_applyPowers(g, left);
    g.monster_order.insert(position, left);
    const int right = g.monsters.size;
    g.monsters.push_back(construct_monster(g, child, hp));
    for (int i = 0; i < g.run.relics.size; ++i) relic_onSpawnMonster(g, i, right);
    monster_init(g, right);
    monster_applyPowers(g, right);
    g.monster_order.insert(position + 2, right);
}

SPIRE_FN inline void block_random_monster(Game& g, const Effect& effect) {
    List<int, monster_capacity> valid{};
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int index = g.monster_order[i];
        const Monster& m = g.monsters[index];
        if (index != effect.source && m.intent != Intent::Escape && !m.creature.dying)
            valid.push_back(index);
    }
    const int target = valid.empty() ? effect.source : valid[g.rng.ai.random(valid.size - 1)];
    gain_block(g, target, effect.amount);
}
}
