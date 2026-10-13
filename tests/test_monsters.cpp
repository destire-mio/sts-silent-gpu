#include "core/engine.hpp"
#include <cstdlib>
#include <cstdio>

using namespace spire;

void require(bool condition, const char* expression, int line) {
    if (!condition) {
        std::fprintf(stderr, "test_monsters.cpp:%d: %s\n", line, expression);
        std::exit(1);
    }
}
#define CHECK(condition) require((condition), #condition, __LINE__)

void execute_next(Game& game) {
    Effect effect = game.manager.actions.pop_front();
    execute_effect(game, effect);
}

void encounter_construction() {
    constexpr int counts[] = {1, 1, 2, 2, 1, 4, 1, 1, 5, 2, 2, 1, 3, 2};
    constexpr int hp_calls[] = {1, 1, 4, 2, 1, 4, 1, 1, 5, 7, 6, 1, 6, 2};
    constexpr int misc_calls[] = {0, 0, 2, 1, 0, 4, 0, 1, 5, 4, 3, 0, 3, 0};
    for (int seed = 0; seed < 100; ++seed) {
        for (int id = 0; id < static_cast<int>(EncounterId::Count); ++id) {
            Game g{};
            g.rng.generate_seeds(seed);
            build_encounter(g, static_cast<EncounterId>(id));
            CHECK(g.monsters.size == counts[id]);
            CHECK(g.monster_order.size == counts[id]);
            CHECK(g.rng.monster_hp.counter == hp_calls[id]);
            CHECK(g.rng.misc.counter == misc_calls[id]);
            CHECK(g.unseeded.counter == (id == static_cast<int>(EncounterId::ExordiumThugs) ||
                id == static_cast<int>(EncounterId::ExordiumWildlife) ? 4 : 0));
            int louses = 0;
            for (int i = 0; i < g.monsters.size; ++i) {
                CHECK(g.monster_order[i] == i);
                const Monster& m = g.monsters[i];
                const MonsterData data = monster_stats(m.id);
                CHECK(m.creature.hp >= data.hp_min && m.creature.hp <= data.hp_max);
                roll_monster_move(g, i);
                monster_pre_battle(g, i);
                if (m.id == MonsterId::LouseNormal || m.id == MonsterId::LouseDefensive) ++louses;
            }
            CHECK(g.rng.ai.counter == counts[id]);
            CHECK(g.rng.monster_hp.counter == hp_calls[id] + louses);
        }
    }
}

void forced_move_history() {
    Game g{};
    g.rng.generate_seeds(123);
    append_monster(g, construct_monster(g, MonsterId::LouseNormal));
    get_monster_move(g, 0, 0);
    CHECK(g.monsters[0].pending_move == 4);
    get_monster_move(g, 0, 0);
    CHECK(g.monsters[0].pending_move == 3);
    get_monster_move(g, 0, 99);
    CHECK(g.monsters[0].pending_move == 3);
    get_monster_move(g, 0, 99);
    CHECK(g.monsters[0].pending_move == 4);
    CHECK(g.rng.ai.counter == 0);
    g.monsters.clear();
    g.monster_order.clear();
    append_monster(g, construct_monster(g, MonsterId::SlaverRed));
    get_monster_move(g, 0, 99);
    CHECK(g.monsters[0].pending_move == 1);
    get_monster_move(g, 0, 99);
    CHECK(g.monsters[0].pending_move == 2);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    CHECK(g.monsters[0].used_special);
    get_monster_move(g, 0, 99);
    CHECK(g.monsters[0].pending_move == 1);
    get_monster_move(g, 0, 99);
    CHECK(g.monsters[0].pending_move == 1);
    get_monster_move(g, 0, 99);
    CHECK(g.monsters[0].pending_move == 3);
}

void deterministic_turns() {
    Game g{};
    g.rng.generate_seeds(321);
    append_monster(g, construct_monster(g, MonsterId::AcidSlime_S));
    roll_monster_move(g, 0);
    CHECK(g.monsters[0].pending_move == 2);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    CHECK(g.monsters[0].pending_move == 1);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    CHECK(g.monsters[0].pending_move == 2);
    CHECK(g.rng.ai.counter == 1);
    g.monsters.clear();
    g.monster_order.clear();
    g.manager.actions.clear();
    append_monster(g, construct_monster(g, MonsterId::GremlinWizard));
    roll_monster_move(g, 0);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    CHECK(g.monsters[0].pending_move == 2);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    const Effect change = g.manager.actions.pop_front();
    CHECK(change.kind == EffectKind::SetMove && change.amount == 1);
    set_monster_move(g, 0, change.amount, static_cast<Intent>(change.extra), change.card);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    CHECK(g.monsters[0].pending_move == 1);
    CHECK(g.manager.actions[0].kind == EffectKind::Damage);
    CHECK(g.manager.actions[0].amount == 30);
    CHECK(g.rng.ai.counter == 2);
}

void split_and_target_identity() {
    Game g{};
    g.rng.generate_seeds(42);
    append_monster(g, construct_monster(g, MonsterId::SpikeSlime_L));
    append_monster(g, construct_monster(g, MonsterId::Cultist));
    roll_monster_move(g, 0);
    const int hp_calls = g.rng.monster_hp.counter;
    const int ai_calls = g.rng.ai.counter;
    const int half = g.monsters[0].creature.max_hp / 2;
    g.monsters[0].creature.hp = half;
    monster_on_damage(g, 0);
    CHECK(g.monsters[0].pending_move == 3);
    CHECK(g.monsters[0].split_triggered);
    CHECK(g.manager.actions.size == 1);
    const int history = g.monsters[0].move_history.size;
    monster_on_damage(g, 0);
    CHECK(g.monsters[0].move_history.size == history);
    split_monster(g, 0);
    CHECK(g.monsters.size == 4);
    CHECK(g.monster_order[0] == 2 && g.monster_order[1] == 0 && g.monster_order[2] == 1 && g.monster_order[3] == 3);
    CHECK(g.monsters[2].creature.draw_x < g.monsters[0].creature.draw_x);
    CHECK(g.monsters[3].creature.draw_x > g.monsters[1].creature.draw_x);
    CHECK(g.monsters[0].creature.dead);
    CHECK(g.monsters[1].id == MonsterId::Cultist);
    CHECK(g.monsters[2].creature.hp == half && g.monsters[3].creature.hp == half);
    CHECK(g.rng.monster_hp.counter == hp_calls);
    CHECK(g.rng.ai.counter == ai_calls + 2);
}

void shield_targeting() {
    Game g{};
    g.rng.generate_seeds(1);
    append_monster(g, construct_monster(g, MonsterId::GremlinTsundere));
    Effect e{};
    e.kind = EffectKind::BlockRandomMonster;
    e.source = 0;
    e.amount = 11;
    block_random_monster(g, e);
    CHECK(g.monsters[0].creature.block == 11 && g.rng.ai.counter == 0);
    append_monster(g, construct_monster(g, MonsterId::GremlinThief));
    block_random_monster(g, e);
    CHECK(g.monsters[1].creature.block == 11 && g.rng.ai.counter == 1);
    g.monsters[1].intent = Intent::Escape;
    block_random_monster(g, e);
    CHECK(g.monsters[0].creature.block == 22 && g.rng.ai.counter == 1);
}

void lethal_attack_keeps_published_intent() {
    Game g{};
    g.rng.generate_seeds(1);
    g.screen = Screen::Combat;
    g.player.creature.hp = 1;
    append_monster(g, construct_monster(g, MonsterId::AcidSlime_S));
    set_monster_move(g, 0, 1, Intent::Attack, 4);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    CHECK(g.monsters[0].move == 1);
    CHECK(g.monsters[0].intent == Intent::Attack);
    CHECK(g.monsters[0].pending_move == 2);
    CHECK(g.monsters[0].pending_intent == Intent::Debuff);
    CHECK(g.monsters[0].move_history[g.monsters[0].move_history.size - 1] == 2);
    CHECK(g.manager.actions[0].kind == EffectKind::AnimateSlowAttack);
    execute_next(g);
    CHECK(g.manager.actions[0].kind == EffectKind::Damage);
    execute_next(g);
    CHECK(g.screen == Screen::Death);
    CHECK(g.monsters[0].move == 1);
    CHECK(g.monsters[0].pending_move == 2);
}

void looter_bookkeeping() {
    Game g{};
    g.rng.generate_seeds(1);
    g.run.gold = 3;
    g.player.creature.hp = 70;
    g.player.creature.block = 20;
    append_monster(g, construct_monster(g, MonsterId::Looter));
    roll_monster_move(g, 0);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    CHECK(g.manager.actions[0].kind == EffectKind::AnimateSlowAttack);
    execute_next(g);
    execute_next(g);
    CHECK(g.monsters[0].stolen_gold == 3);
    CHECK(g.run.gold == 3);
    execute_next(g);
    CHECK(g.run.gold == 0);
    CHECK(g.player.creature.hp == 70);
    CHECK(g.player.creature.block == 9);
    g.manager.actions.clear();
    set_monster_move(g, 0, 3, Intent::Escape);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    CHECK(g.mugged);
    CHECK(!g.monsters[0].creature.escaped);
    execute_next(g);
    CHECK(g.monsters[0].creature.escaped);
}

int main() {
    encounter_construction();
    forced_move_history();
    deterministic_turns();
    split_and_target_identity();
    shield_targeting();
    lethal_attack_keeps_published_intent();
    looter_bookkeeping();
    std::puts("monster checks passed");
}
