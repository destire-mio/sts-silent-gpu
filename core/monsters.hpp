#pragma once
#include "combat.hpp"

namespace spire {
struct MonsterData {
    int hp_min;
    int hp_max;
    int damage[2];
    int block;
    int magic;
};
SPIRE_FN inline MonsterData monster_stats(MonsterId id) {
    constexpr MonsterData data[] = {
    {50, 56, {6, 0}, 0, 5},
    {42, 46, {12, 7}, 9, 5},
    {11, 16, {0, 0}, 0, 4},
    {12, 18, {0, 0}, 0, 2},
    {9, 13, {4, 0}, 0, 1},
    {29, 34, {8, 12}, 0, 1},
    {68, 72, {12, 18}, 0, 2},
    {11, 15, {6, 0}, 0, 0},
    {29, 34, {10, 0}, 0, 1},
    {67, 73, {18, 0}, 0, 3},
    {24, 28, {6, 0}, 0, 5},
    {48, 52, {13, 8}, 0, 2},
    {48, 52, {14, 9}, 0, 2},
    {46, 50, {11, 14}, 6, 20},
    {21, 25, {5, 0}, 0, 2},
    {11, 15, {10, 0}, 0, 0},
    {14, 18, {5, 0}, 0, 1},
    {13, 17, {8, 0}, 11, 0},
    {22, 26, {30, 0}, 0, 0}
    };
    return data[static_cast<int>(id)];
}

SPIRE_FN inline Monster construct_monster(Game& g, MonsterId id, int hp_override = 0) {
    const MonsterData data = monster_stats(id);
    Monster m{};
    m.id = id;
    m.creature.hp = hp_override == 0 ? g.rng.monster_hp.random(data.hp_min, data.hp_max) : hp_override;
    m.creature.max_hp = m.creature.hp;
    m.damage[0] = data.damage[0];
    m.damage[1] = data.damage[1];
    m.block_amount = data.block;
    m.magic = data.magic;
    if (id == MonsterId::LouseNormal || id == MonsterId::LouseDefensive)
        m.damage[0] = g.rng.monster_hp.random(6, 8);
    if (id == MonsterId::AcidSlime_L || id == MonsterId::SpikeSlime_L)
        m.creature.powers.push_back(Power{PowerId::Split, -1});
    if (id == MonsterId::GremlinWizard) m.turn_count = 1;
    return m;
}

SPIRE_FN inline void append_monster(Game& g, const Monster& monster) {
    g.monster_order.push_back(g.monsters.size);
    g.monsters.push_back(monster);
}

SPIRE_FN inline Monster random_louse(Game& g) {
    return construct_monster(g, g.rng.misc.randomBoolean() ? MonsterId::LouseNormal : MonsterId::LouseDefensive);
}

SPIRE_FN inline Monster weak_wildlife(Game& g) {
    // MonsterHelper.bottomGetWeakWildlife constructs every candidate before choosing.
    const Monster louse = random_louse(g);
    const Monster spike = construct_monster(g, MonsterId::SpikeSlime_M);
    const Monster acid = construct_monster(g, MonsterId::AcidSlime_M);
    const int pick = g.rng.misc.random(0, 2);
    return pick == 0 ? louse : pick == 1 ? spike : acid;
}

SPIRE_FN inline Monster strong_humanoid(Game& g) {
    // MonsterHelper.bottomGetStrongHumanoid consumes discarded candidates' HP rolls.
    const Monster cultist = construct_monster(g, MonsterId::Cultist);
    const Monster slaver = construct_monster(g, g.rng.misc.randomBoolean() ? MonsterId::SlaverRed : MonsterId::SlaverBlue);
    const Monster looter = construct_monster(g, MonsterId::Looter);
    const int pick = g.rng.misc.random(0, 2);
    return pick == 0 ? cultist : pick == 1 ? slaver : looter;
}

SPIRE_FN inline void build_encounter(Game& g, EncounterId encounter) {
    g.monsters.clear();
    g.monster_order.clear();
    g.encounter = encounter;
    switch (encounter) {
    case EncounterId::Cultist: append_monster(g, construct_monster(g, MonsterId::Cultist)); break;
    case EncounterId::JawWorm: append_monster(g, construct_monster(g, MonsterId::JawWorm)); break;
    case EncounterId::BlueSlaver: append_monster(g, construct_monster(g, MonsterId::SlaverBlue)); break;
    case EncounterId::RedSlaver: append_monster(g, construct_monster(g, MonsterId::SlaverRed)); break;
    case EncounterId::Looter: append_monster(g, construct_monster(g, MonsterId::Looter)); break;
    case EncounterId::TwoLouse:
    case EncounterId::ThreeLouse:
        for (int i = 0; i < (encounter == EncounterId::TwoLouse ? 2 : 3); ++i)
            append_monster(g, random_louse(g));
        break;
    case EncounterId::SmallSlimes:
        if (g.rng.misc.randomBoolean()) {
            append_monster(g, construct_monster(g, MonsterId::SpikeSlime_S));
            append_monster(g, construct_monster(g, MonsterId::AcidSlime_M));
        } else {
            append_monster(g, construct_monster(g, MonsterId::AcidSlime_S));
            append_monster(g, construct_monster(g, MonsterId::SpikeSlime_M));
        }
        break;
    case EncounterId::LargeSlime:
        append_monster(g, construct_monster(g, g.rng.misc.randomBoolean() ? MonsterId::AcidSlime_L : MonsterId::SpikeSlime_L));
        break;
    case EncounterId::TwoFungiBeasts:
        append_monster(g, construct_monster(g, MonsterId::FungiBeast));
        append_monster(g, construct_monster(g, MonsterId::FungiBeast));
        break;
    case EncounterId::LotsOfSlimes: {
        List<MonsterId, 5> pool{};
        pool.push_back(MonsterId::SpikeSlime_S);
        pool.push_back(MonsterId::SpikeSlime_S);
        pool.push_back(MonsterId::SpikeSlime_S);
        pool.push_back(MonsterId::AcidSlime_S);
        pool.push_back(MonsterId::AcidSlime_S);
        while (!pool.empty()) {
            const MonsterId id = pool.erase(g.rng.misc.random(pool.size - 1));
            append_monster(g, construct_monster(g, id));
        }
        break;
    }
    case EncounterId::GremlinGang: {
        List<MonsterId, 8> pool{};
        pool.push_back(MonsterId::GremlinWarrior);
        pool.push_back(MonsterId::GremlinWarrior);
        pool.push_back(MonsterId::GremlinThief);
        pool.push_back(MonsterId::GremlinThief);
        pool.push_back(MonsterId::GremlinFat);
        pool.push_back(MonsterId::GremlinFat);
        pool.push_back(MonsterId::GremlinTsundere);
        pool.push_back(MonsterId::GremlinWizard);
        for (int i = 0; i < 4; ++i) {
            const MonsterId id = pool.erase(g.rng.misc.random(pool.size - 1));
            append_monster(g, construct_monster(g, id));
        }
        break;
    }
    case EncounterId::ExordiumThugs:
        append_monster(g, weak_wildlife(g));
        append_monster(g, strong_humanoid(g));
        break;
    case EncounterId::ExordiumWildlife: {
        // MonsterHelper.bottomGetStrongWildlife constructs both before choosing.
        const Monster fungi = construct_monster(g, MonsterId::FungiBeast);
        const Monster jaw = construct_monster(g, MonsterId::JawWorm);
        append_monster(g, g.rng.misc.random(0, 1) == 0 ? fungi : jaw);
        append_monster(g, weak_wildlife(g));
        break;
    }
    case EncounterId::Count: SPIRE_ASSERT(false); break;
    }
}

SPIRE_FN inline bool last_move(const Monster& m, int move) {
    return m.move_history.size > 0 && m.move_history[m.move_history.size - 1] == move;
}

SPIRE_FN inline bool last_two_moves(const Monster& m, int move) {
    return m.move_history.size >= 2 && m.move_history[m.move_history.size - 1] == move &&
        m.move_history[m.move_history.size - 2] == move;
}

SPIRE_FN inline void set_monster_move(Game& g, int index, int move, Intent intent, int base_damage = -1, int hits = 1) {
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

SPIRE_FN inline void get_monster_move(Game& g, int index, int num) {
    Monster& m = g.monsters[index];
    switch (m.id) {
    case MonsterId::Cultist:
        if (m.first_move) {
            m.first_move = false;
            set_monster_move(g, index, 3, Intent::Buff);
        } else set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        break;
    case MonsterId::JawWorm:
        if (m.first_move) {
            m.first_move = false;
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        } else if (num < 25) {
            if (!last_move(m, 1)) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
            else if (g.rng.ai.randomBoolean(0.5625f)) set_monster_move(g, index, 2, Intent::DefendBuff);
            else set_monster_move(g, index, 3, Intent::AttackDefend, m.damage[1]);
        } else if (num < 55) {
            if (!last_two_moves(m, 3)) set_monster_move(g, index, 3, Intent::AttackDefend, m.damage[1]);
            else if (g.rng.ai.randomBoolean(0.357f)) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
            else set_monster_move(g, index, 2, Intent::DefendBuff);
        } else if (!last_move(m, 2)) set_monster_move(g, index, 2, Intent::DefendBuff);
        else if (g.rng.ai.randomBoolean(0.416f)) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 3, Intent::AttackDefend, m.damage[1]);
        break;
    case MonsterId::LouseNormal:
    case MonsterId::LouseDefensive:
        if ((num < 25 && !last_move(m, 4)) || (num >= 25 && last_two_moves(m, 3)))
            set_monster_move(g, index, 4, m.id == MonsterId::LouseNormal ? Intent::Buff : Intent::Debuff);
        else set_monster_move(g, index, 3, Intent::Attack, m.damage[0]);
        break;
    case MonsterId::AcidSlime_S:
        // AcidSlime_S.getMove checks for two attacks, then chooses another attack.
        if (last_two_moves(m, 1)) set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 2, Intent::Debuff);
        break;
    case MonsterId::AcidSlime_M:
    case MonsterId::AcidSlime_L: {
        const bool large = m.id == MonsterId::AcidSlime_L;
        if (num < 40) {
            if (!last_two_moves(m, 1)) set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
            else {
                const bool attack = large ? g.rng.ai.randomBoolean(0.6f) : g.rng.ai.randomBoolean();
                if (attack) set_monster_move(g, index, 2, Intent::Attack, m.damage[1]);
                else set_monster_move(g, index, 4, Intent::Debuff);
            }
        } else if (num < (large ? 70 : 80)) {
            if (!last_two_moves(m, 2)) set_monster_move(g, index, 2, Intent::Attack, m.damage[1]);
            else if (g.rng.ai.randomBoolean(large ? 0.6f : 0.5f))
                set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
            else set_monster_move(g, index, 4, Intent::Debuff);
        } else if (!last_move(m, 4)) set_monster_move(g, index, 4, Intent::Debuff);
        else if (g.rng.ai.randomBoolean(0.4f)) set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
        else set_monster_move(g, index, 2, Intent::Attack, m.damage[1]);
        break;
    }
    case MonsterId::SpikeSlime_S:
    case MonsterId::Looter:
    case MonsterId::GremlinWarrior:
    case MonsterId::GremlinThief:
        set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        break;
    case MonsterId::SpikeSlime_M:
    case MonsterId::SpikeSlime_L:
        if ((num < 30 && !last_two_moves(m, 1)) || (num >= 30 && last_move(m, 4)))
            set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
        else set_monster_move(g, index, 4, Intent::Debuff);
        break;
    case MonsterId::FungiBeast:
        if ((num < 60 && !last_two_moves(m, 1)) || (num >= 60 && last_move(m, 2)))
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 2, Intent::Buff);
        break;
    case MonsterId::SlaverBlue:
        if ((num >= 40 && !last_two_moves(m, 1)) || last_move(m, 4))
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 4, Intent::AttackDebuff, m.damage[1]);
        break;
    case MonsterId::SlaverRed:
        if (m.first_move) {
            m.first_move = false;
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        } else if (num >= 75 && !m.used_special) set_monster_move(g, index, 2, Intent::StrongDebuff);
        else if ((num >= 55 && m.used_special && !last_two_moves(m, 1)) || last_move(m, 3))
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 3, Intent::AttackDebuff, m.damage[1]);
        break;
    case MonsterId::GremlinFat:
        set_monster_move(g, index, 2, Intent::AttackDebuff, m.damage[0]);
        break;
    case MonsterId::GremlinTsundere: set_monster_move(g, index, 1, Intent::Defend); break;
    case MonsterId::GremlinWizard: set_monster_move(g, index, 2, Intent::Unknown); break;
    case MonsterId::Count: SPIRE_ASSERT(false); break;
    }
}

SPIRE_FN inline void roll_monster_move(Game& g, int index) {
    get_monster_move(g, index, g.rng.ai.random(99));
}

SPIRE_FN inline void monster_pre_battle(Game& g, int index) {
    const Monster& m = g.monsters[index];
    switch (m.id) {
    case MonsterId::LouseNormal:
    case MonsterId::LouseDefensive:
        queue_power(g, index, index, PowerId::CurlUp, g.rng.monster_hp.random(9, 12));
        break;
    case MonsterId::FungiBeast: queue_power(g, index, index, PowerId::SporeCloud, 2); break;
    case MonsterId::Looter: queue_power(g, index, index, PowerId::Thievery, m.magic); break;
    case MonsterId::GremlinWarrior: queue_power(g, index, index, PowerId::Angry, m.magic); break;
    default: break;
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

SPIRE_FN inline void monster_take_turn(Game& g, int index) {
    Monster& m = g.monsters[index];
    switch (m.id) {
    case MonsterId::Cultist:
        if (m.move == 3) queue_power(g, index, index, PowerId::Ritual, m.magic);
        else queue_damage(g, index, player_target, m.damage[0]);
        queue_monster_roll(g, index);
        break;
    case MonsterId::JawWorm:
        if (m.move == 1) queue_damage(g, index, player_target, m.damage[0]);
        else if (m.move == 2) {
            queue_power(g, index, index, PowerId::Strength, m.magic);
            queue_block(g, index, m.block_amount);
        } else {
            queue_damage(g, index, player_target, m.damage[1]);
            queue_block(g, index, 5);
        }
        queue_monster_roll(g, index);
        break;
    case MonsterId::LouseNormal:
    case MonsterId::LouseDefensive:
        if (m.move == 3) queue_damage(g, index, player_target, m.damage[0]);
        else if (m.id == MonsterId::LouseNormal) queue_power(g, index, index, PowerId::Strength, m.magic);
        else queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
        queue_monster_roll(g, index);
        break;
    case MonsterId::AcidSlime_S:
        if (m.move == 1) {
            queue_damage(g, index, player_target, m.damage[0]);
            set_monster_move(g, index, 2, Intent::Debuff);
        } else {
            queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        }
        break;
    case MonsterId::AcidSlime_M:
    case MonsterId::AcidSlime_L:
    case MonsterId::SpikeSlime_M:
    case MonsterId::SpikeSlime_L: {
        const bool acid = m.id == MonsterId::AcidSlime_M || m.id == MonsterId::AcidSlime_L;
        const bool large = m.id == MonsterId::AcidSlime_L || m.id == MonsterId::SpikeSlime_L;
        if (m.move == 3) {
            Effect e{};
            e.kind = EffectKind::Split;
            e.target = index;
            add_bottom(g, e);
            set_monster_move(g, index, 3, Intent::Unknown);
        } else if (m.move == 4) queue_power(g, index, player_target, acid ? PowerId::Weak : PowerId::Frail, m.magic, true);
        else {
            queue_damage(g, index, player_target, m.damage[m.move == 2 ? 1 : 0]);
            if (m.move == 1) queue_slimed(g, large ? 2 : 1);
        }
        // SpikeSlime_L.takeTurn rolls even after splitting; AcidSlime_L does not.
        if (!(acid && m.move == 3)) queue_monster_roll(g, index);
        break;
    }
    case MonsterId::SpikeSlime_S:
        queue_damage(g, index, player_target, m.damage[0]);
        queue_monster_roll(g, index);
        break;
    case MonsterId::FungiBeast:
        if (m.move == 1) queue_damage(g, index, player_target, m.damage[0]);
        else queue_power(g, index, index, PowerId::Strength, m.magic);
        queue_monster_roll(g, index);
        break;
    case MonsterId::SlaverBlue:
        queue_damage(g, index, player_target, m.damage[m.move == 1 ? 0 : 1]);
        if (m.move == 4) queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
        queue_monster_roll(g, index);
        break;
    case MonsterId::SlaverRed:
        if (m.move == 2) {
            queue_power(g, index, player_target, PowerId::Entangled, 1, true);
            m.used_special = true;
        } else {
            queue_damage(g, index, player_target, m.damage[m.move == 1 ? 0 : 1]);
            if (m.move == 3) queue_power(g, index, player_target, PowerId::Vulnerable, m.magic, true);
        }
        queue_monster_roll(g, index);
        break;
    case MonsterId::Looter:
        if (m.move == 1 || m.move == 4) {
            const int move = m.move;
            // Looter.takeTurn consumes aiRng for its first Mug speech even without rendering.
            if (move == 1 && m.turn_count == 0) g.rng.ai.randomBoolean(0.6f);
            Effect steal{};
            steal.kind = EffectKind::StealGold;
            steal.target = index;
            steal.amount = m.magic;
            add_bottom(g, steal);
            queue_damage(g, index, player_target, m.damage[move == 1 ? 0 : 1], DamageType::Normal, m.magic);
            ++m.turn_count;
            if (move == 4) set_monster_move(g, index, 2, Intent::Defend);
            else if (m.turn_count == 2) {
                if (g.rng.ai.randomBoolean(0.5f)) set_monster_move(g, index, 2, Intent::Defend);
                else queue_monster_move(g, index, 4, Intent::Attack, m.damage[1]);
            } else queue_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        } else if (m.move == 2) {
            queue_block(g, index, m.block_amount);
            queue_monster_move(g, index, 3, Intent::Escape);
        } else {
            g.mugged = true;
            Effect e{};
            e.kind = EffectKind::Escape;
            e.target = index;
            add_bottom(g, e);
            queue_monster_move(g, index, 3, Intent::Escape);
        }
        break;
    case MonsterId::GremlinWarrior:
    case MonsterId::GremlinThief:
        queue_damage(g, index, player_target, m.damage[0]);
        queue_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        break;
    case MonsterId::GremlinFat:
        queue_damage(g, index, player_target, m.damage[0]);
        queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
        queue_power(g, index, player_target, PowerId::Frail, m.magic, true);
        queue_monster_roll(g, index);
        break;
    case MonsterId::GremlinTsundere:
        if (m.move == 1) {
            Effect e{};
            e.kind = EffectKind::BlockRandomMonster;
            e.source = index;
            e.amount = m.block_amount;
            add_bottom(g, e);
            int alive = 0;
            for (int i = 0; i < g.monster_order.size; ++i) {
                const Creature& c = g.monsters[g.monster_order[i]].creature;
                if (!c.dying && !c.escaped) ++alive;
            }
            if (alive > 1) set_monster_move(g, index, 1, Intent::Defend);
            else set_monster_move(g, index, 2, Intent::Attack, m.damage[0]);
        } else {
            queue_damage(g, index, player_target, m.damage[0]);
            queue_monster_move(g, index, 2, Intent::Attack, m.damage[0]);
        }
        break;
    case MonsterId::GremlinWizard:
        if (m.move == 2) {
            ++m.turn_count;
            if (m.turn_count == 3) queue_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
            else set_monster_move(g, index, 2, Intent::Unknown);
        } else {
            m.turn_count = 0;
            queue_damage(g, index, player_target, m.damage[0]);
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        }
        break;
    case MonsterId::Count: SPIRE_ASSERT(false); break;
    }
}

SPIRE_FN inline void monster_on_damage(Game& g, int index) {
    Monster& m = g.monsters[index];
    if ((m.id == MonsterId::AcidSlime_L || m.id == MonsterId::SpikeSlime_L) &&
        !m.creature.dying && static_cast<float>(m.creature.hp) <= static_cast<float>(m.creature.max_hp) / 2.0f &&
        m.move != 3 && !m.split_triggered) {
        set_monster_move(g, index, 3, Intent::Unknown);
        create_monster_intent(g, index);
        queue_monster_move(g, index, 3, Intent::Unknown);
        m.split_triggered = true;
    }
}

SPIRE_FN inline void split_monster(Game& g, int index) {
    Monster& parent = g.monsters[index];
    const MonsterId child = parent.id == MonsterId::AcidSlime_L ? MonsterId::AcidSlime_M : MonsterId::SpikeSlime_M;
    const int hp = parent.creature.hp;
    parent.creature.hp = 0;
    parent.creature.dying = true;
    parent.creature.dead = true;
    int position = 0;
    while (g.monster_order[position] != index) ++position;
    const int left = g.monsters.size;
    g.monsters.push_back(construct_monster(g, child, hp));
    roll_monster_move(g, left);
    g.monster_order.insert(position, left);
    const int right = g.monsters.size;
    g.monsters.push_back(construct_monster(g, child, hp));
    roll_monster_move(g, right);
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
