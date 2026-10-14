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
    Effect effect = game.action_objects[game.manager.actions.pop_front()];
    execute_effect(game, effect);
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
    CHECK(g.monsters[0].state.SlaverRed.usedEntangle);
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
    clear_actions(g, g.manager.actions);
    append_monster(g, construct_monster(g, MonsterId::GremlinWizard));
    roll_monster_move(g, 0);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    CHECK(g.monsters[0].pending_move == 2);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    const Effect change = g.action_objects[g.manager.actions.pop_front()];
    CHECK(change.kind == EffectKind::SetMove && change.amount == 1);
    set_monster_move(g, 0, change.amount, static_cast<Intent>(change.extra), change.card);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    CHECK(g.monsters[0].pending_move == 1);
    CHECK(g.action_objects[g.manager.actions[0]].kind == EffectKind::Damage);
    CHECK(g.action_objects[g.manager.actions[0]].amount == 30);
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
    CHECK(g.monsters[0].state.SpikeSlime_L.splitTriggered);
    CHECK(g.manager.actions.size == 2);
    const int history = g.monsters[0].move_history.size;
    monster_on_damage(g, 0);
    CHECK(g.monsters[0].move_history.size == history);
    clear_actions(g, g.manager.actions);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    while (!g.manager.actions.empty()) execute_next(g);
    CHECK(g.monsters.size == 4);
    CHECK(g.monster_order[0] == 2 && g.monster_order[1] == 0 && g.monster_order[2] == 1 && g.monster_order[3] == 3);
    CHECK(g.monsters[2].creature.draw_x < g.monsters[0].creature.draw_x);
    CHECK(g.monsters[3].creature.draw_x > g.monsters[1].creature.draw_x);
    CHECK(g.monsters[0].creature.dead);
    CHECK(g.monsters[1].id == MonsterId::Cultist);
    CHECK(g.monsters[2].creature.hp == half && g.monsters[3].creature.hp == half);
    CHECK(g.rng.monster_hp.counter == hp_calls);
    CHECK(g.rng.ai.counter == ai_calls + 3);
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
    CHECK(g.move_histories.last(g.monsters[0].move_history) == 2);
    CHECK(g.action_objects[g.manager.actions[0]].kind == EffectKind::Damage);
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
    execute_next(g);
    CHECK(g.monsters[0].state.Looter.stolenGold == 3);
    CHECK(g.run.gold == 3);
    execute_next(g);
    CHECK(g.run.gold == 0);
    CHECK(g.player.creature.hp == 70);
    CHECK(g.player.creature.block == 9);
    clear_actions(g, g.manager.actions);
    set_monster_move(g, 0, 3, Intent::Escape);
    create_monster_intent(g, 0);
    monster_take_turn(g, 0);
    CHECK(g.mugged);
    CHECK(!g.monsters[0].creature.escaped);
    execute_next(g);
    CHECK(g.monsters[0].creature.escaped);
}

void suicide_preserves_boss_progress() {
    constexpr bool triggers[]{false, true};
    for (bool trigger_relics : triggers) {
        Game g{};
        g.rng.generate_seeds(42);
        append_monster(g, construct_monster(g, MonsterId::Hexaghost));
        queue_suicide(g, 0, trigger_relics);
        execute_next(g);
        CHECK(g.monsters[0].creature.hp == 0);
        CHECK(g.monsters[0].creature.dying);
        CHECK(g.run.boss_count == 0);
    }
}

int main() {
    forced_move_history();
    deterministic_turns();
    split_and_target_identity();
    shield_targeting();
    lethal_attack_keeps_published_intent();
    looter_bookkeeping();
    suicide_preserves_boss_progress();
    std::puts("monster checks passed");
}
