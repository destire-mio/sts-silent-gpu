#pragma once
#include "../hooks.hpp"
#include "../actions/spawn_monster.hpp"

namespace spire {
SPIRE_FN inline void position_monster(Monster& monster, float offset_x) {
    monster.offset_x = offset_x;
    monster.creature.draw_x = 1280.0f * 0.75f + offset_x * (1280.0f / 1920.0f);
}

SPIRE_FN inline Monster make_monster(Game& g, MonsterId id, const MonsterData& data, int hp_override) {
    Monster m{};
    m.id = id;
    position_monster(m, 0.0f);
    monster_initialize_state(m);
    m.creature.hp = hp_override == 0 ? g.rng.monster_hp.random(data.hp_min, data.hp_max) : hp_override;
    m.creature.max_hp = m.creature.hp;
    m.damage_count = data.damage_count;
    for (int i = 0; i < m.damage_count; ++i) {
        m.damage[i] = data.damage[i];
        m.damage_output[i] = data.damage[i];
    }
    m.block_amount = data.block;
    m.magic = data.magic;
    return m;
}

SPIRE_FN inline void append_monster(Game& g, Monster monster, float offset_x = 0.0f) {
    position_monster(monster, monster.offset_x + offset_x);
    g.monster_order.push_back(g.monsters.size);
    g.monsters.push_back(monster);
}

SPIRE_FN inline bool last_move(const Game& g, const Monster& m, int move) {
    return m.move_history.size > 0 && g.move_histories.last(m.move_history) == move;
}

SPIRE_FN inline bool last_two_moves(const Game& g, const Monster& m, int move) {
    return m.move_history.size >= 2 && g.move_histories.last(m.move_history) == move &&
        g.move_histories.last(m.move_history, 1) == move;
}

SPIRE_FN inline void set_monster_move(Game& g, int index, int move, Intent intent, int base_damage, int hits) {
    Monster& m = g.monsters[index];
    m.pending_move = move;
    m.move_name = 0;
    m.pending_intent = intent;
    m.pending_base_damage = base_damage;
    m.pending_hits = hits > 1 ? hits : 0;
    m.pending_multi = hits > 1;
    if (move != -1) g.move_histories.append(m.move_history, move);
}

SPIRE_FN inline void create_monster_intent(Game& g, int index) {
    Monster& m = g.monsters[index];
    m.move = m.pending_move;
    m.intent = m.pending_intent;
    m.intent_base_damage = m.pending_base_damage;
    if (m.pending_base_damage >= 0) {
        m.intent_damage = calculate_intent_damage(g, index, m.pending_base_damage);
        m.intent_multi = m.pending_multi;
        m.intent_hits = m.pending_multi ? m.pending_hits : -1;
    }
}

SPIRE_FN inline void queue_monster_move(Game& g, int index, int move, Intent intent, int damage = -1, int hits = 0) {
    Effect e{};
    e.kind = EffectKind::SetMove;
    action_initialize_state(e);
    e.target = index;
    e.amount = move;
    e.extra = static_cast<int>(intent);
    e.card = damage;
    e.state.SetMove.the_multiplier = hits;
    e.state.SetMove.is_multiplier = hits > 0;
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
    e.card = g.cards.size;
    g.cards.push_back(make_card(CardId::Slimed));
    g.cards[e.card].uuid = g.next_card_uuid++;
    e.amount = amount;
    add_bottom(g, e);
}

SPIRE_FN inline void queue_monster_card(Game& g, CardId card, EffectKind kind, int amount, int upgrades = 0) {
    Effect e{};
    e.kind = kind;
    action_initialize_state(e);
    e.card = create_card(g, make_card(card, upgrades));
    e.amount = kind == EffectKind::MakeTempCardInDiscardAndDeck ? 0 : amount;
    if (kind == EffectKind::MakeTempCardInDrawPile) e.state.MakeTempCardInDrawPile.random_spot = true;
    add_bottom(g, e);
}

SPIRE_FN inline int queue_spawn_monster(Game& g, MonsterId id, float offset_x, bool minion, int hp = 0) {
    const int index = g.monsters.size;
    Monster monster = construct_monster(g, id, hp);
    position_monster(monster, monster.offset_x + offset_x);
    g.monsters.push_back(monster);
    add_bottom(g, units::actions::SpawnMonster::construct(index, minion));
    return index;
}

SPIRE_FN inline void queue_suicide(Game& g, int index, bool trigger_relics) {
    Effect e{};
    e.kind = EffectKind::Suicide;
    action_initialize_state(e);
    e.target = index;
    e.state.Suicide.suppress_relics = !trigger_relics;
    add_bottom(g, e);
}

SPIRE_FN inline void queue_split(Game& g, int index, MonsterId left, MonsterId right, float left_x, float right_x) {
    Effect e{};
    e.kind = EffectKind::CannotLose;
    add_bottom(g, e);
    queue_suicide(g, index, false);
    const int hp = g.monsters[index].creature.hp;
    queue_spawn_monster(g, left, left_x, false, hp);
    queue_spawn_monster(g, right, right_x, false, hp);
    e.kind = EffectKind::CanLose;
    add_bottom(g, e);
}

SPIRE_FN inline void roll_monster_move(Game& g, int index) {
    monster_rollMove(g, index);
}

SPIRE_FN inline void block_random_monster(Game& g, Effect& effect) {
    List<int, monster_capacity> valid{};
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int index = g.monster_order[i];
        const Monster& m = g.monsters[index];
        if (index != effect.source && m.intent != Intent::Escape && !m.creature.dying)
            valid.push_back(index);
    }
    effect.target = valid.empty() ? effect.source : valid[g.rng.ai.random(valid.size - 1)];
    if (effect.target != no_target) gain_block(g, effect.target, effect.amount);
}
}
