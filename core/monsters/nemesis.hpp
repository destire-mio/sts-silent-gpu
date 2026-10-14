#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Nemesis : MonsterHooks {
    SPIRE_SNAPSHOT("Nemesis.fireDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_STATE(Nemesis, int scytheCooldown; bool firstMove;)
    SPIRE_FN static constexpr MonsterData data() { return {200, 200, {45, 7}, 0, 0, 2, EnemyType::Elite}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 8) stats.hp_min = stats.hp_max = 185;
        if (g.run.ascension < 3) stats.damage[1] = 6;
        Monster m = make_monster(g, MonsterId::Nemesis, stats, hp_override);
        m.state.Nemesis.firstMove = true;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner]; auto& s = m.state.Nemesis;
        --s.scytheCooldown;
        int move;
        if (s.firstMove) { s.firstMove = false; move = num < 50 ? 2 : 4; }
        else if (num < 30) {
            if (!last_move(g, m, 3) && s.scytheCooldown <= 0) move = 3;
            else if (g.rng.ai.randomBoolean()) move = !last_two_moves(g, m, 2) ? 2 : 4;
            else move = !last_move(g, m, 4) ? 4 : 2;
        } else if (num < 65) {
            if (!last_two_moves(g, m, 2)) move = 2;
            else if (g.rng.ai.randomBoolean()) move = s.scytheCooldown > 0 ? 4 : 3;
            else move = 4;
        } else if (!last_move(g, m, 4)) move = 4;
        else if (g.rng.ai.randomBoolean() && s.scytheCooldown <= 0) move = 3;
        else move = 2;
        if (move == 3) { s.scytheCooldown = 2; set_monster_move(g, owner, 3, Intent::Attack, 45); }
        else if (move == 2) set_monster_move(g, owner, 2, Intent::Attack, monster_base_damage(g, m, 1), 3);
        else set_monster_move(g, owner, 4, Intent::Debuff);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        if (m.move == 3) {
            add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e);
            queue_monster_damage(g, owner, player_target, 0);
        }
        else if (m.move == 2) for (int i = 0; i < 3; ++i) queue_monster_damage(g, owner, player_target, 1);
        else {
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e);
            add_bottom(g, actions::VFX::make(owner));
            queue_monster_card(g, CardId::Burn, EffectKind::AddCardDiscard, g.run.ascension >= 18 ? 5 : 3);
        }
        if (!has_power(g, m.creature, PowerId::Intangible)) queue_power(g, owner, owner, PowerId::Intangible, 1);
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void damage(Game& g, int owner, const DamageInfo& info) {
        DamageInfo hit = info;
        if (hit.amount > 0 && has_power(g, g.monsters[owner].creature, PowerId::Intangible)) hit.amount = 1;
        monster_damage_base(g, owner, hit);
    }
};
}
