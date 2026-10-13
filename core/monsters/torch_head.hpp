#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct TorchHead : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {40, 45, {7}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        // TorchHead.<init> rolls the superclass HP before setHp rerolls it.
        g.rng.monster_hp.random(38, 40);
        MonsterData d = data();
        if (g.run.ascension < 9) { d.hp_min = 38; d.hp_max = 40; }
        Monster m = make_monster(g, MonsterId::TorchHead, d, hp);
        m.pending_move = 1; m.pending_intent = Intent::Attack; m.pending_base_damage = 7; g.move_histories.append(m.move_history, 1);
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) { (void)num; set_monster_move(g, owner, 1, Intent::Attack, 7); }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        queue_damage(g, owner, player_target, 7);
        queue_monster_move(g, owner, 1, Intent::Attack, 7);
    }
};
}
