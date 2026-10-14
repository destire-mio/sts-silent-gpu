#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct Taskmaster : MonsterHooks {
    SPIRE_STATE(Taskmaster, int woundCount;)
    SPIRE_FN static constexpr MonsterData data() { return {57, 64, {4, 7}, 0, 0, 2, EnemyType::Elite}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        // Taskmaster.<init> rolls the superclass HP before setHp rerolls it.
        g.rng.monster_hp.random(54, 60);
        MonsterData d = data();
        if (g.run.ascension < 8) { d.hp_min = 54; d.hp_max = 60; }
        Monster m = make_monster(g, MonsterId::Taskmaster, d, hp);
        m.state.Taskmaster.woundCount = g.run.ascension >= 18 ? 3 : g.run.ascension >= 3 ? 2 : 1;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) { (void)num; set_monster_move(g, owner, 2, Intent::AttackDebuff, 7); }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        queue_monster_damage(g, owner, player_target, 1);
        Effect e{}; e.kind = EffectKind::AddCardDiscard; e.card = create_card(g, make_card(CardId::Wound)); e.amount = g.monsters[owner].state.Taskmaster.woundCount;
        add_bottom(g, e);
        if (g.run.ascension >= 18) queue_power(g, owner, owner, PowerId::Strength, 1);
        queue_monster_roll(g, owner);
    }
};
}
