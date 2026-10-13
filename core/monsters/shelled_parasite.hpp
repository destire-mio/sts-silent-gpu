#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct ShelledParasite : MonsterHooks {
    SPIRE_STATE(ShelledParasite, bool firstMove;)
    SPIRE_SNAPSHOT("ShelledParasite.doubleStrikeDmg", scalar, monster.damage[0])
    SPIRE_SNAPSHOT("ShelledParasite.fellDmg", scalar, monster.damage[1])
    SPIRE_SNAPSHOT("ShelledParasite.suckDmg", scalar, monster.damage[2])
    SPIRE_FN static constexpr MonsterData data() { return {70, 75, {7, 21, 12}, 0, 0, 3}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 68; d.hp_max = 72; }
        if (g.run.ascension < 2) { d.damage[0] = 6; d.damage[1] = 18; d.damage[2] = 10; }
        Monster m = make_monster(g, MonsterId::ShelledParasite, d, hp);
        m.state.ShelledParasite.firstMove = true;
        position_monster(m, -20.0f);
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::PlatedArmor, 14);
        queue_block(g, owner, 14);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        if (m.state.ShelledParasite.firstMove) {
            m.state.ShelledParasite.firstMove = false;
            if (g.run.ascension >= 17) { set_monster_move(g, owner, 1, Intent::AttackDebuff, m.damage[1]); return; }
            if (g.rng.ai.randomBoolean()) set_monster_move(g, owner, 2, Intent::Attack, m.damage[0], 2);
            else set_monster_move(g, owner, 3, Intent::AttackBuff, m.damage[2]);
            return;
        }
        if (num < 20 && last_move(m, 1)) num = g.rng.ai.random(20, 99);
        if (num < 20) set_monster_move(g, owner, 1, Intent::AttackDebuff, m.damage[1]);
        else if (num < 60 ? !last_two_moves(m, 2) : last_two_moves(m, 3)) set_monster_move(g, owner, 2, Intent::Attack, m.damage[0], 2);
        else set_monster_move(g, owner, 3, Intent::AttackBuff, m.damage[2]);
    }
    SPIRE_FN static void changeState(Game& g, int owner, const char* key) {
        if (same_text(key, "ARMOR_BREAK")) {
            set_monster_move(g, owner, 4, Intent::Stun);
            monster_createIntent(g, owner);
        }
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        switch (m.move) {
        case 1:
            queue_damage(g, owner, player_target, m.damage[1]);
            queue_power(g, owner, player_target, PowerId::Frail, 2, true);
            break;
        case 2:
            queue_damage(g, owner, player_target, m.damage[0]);
            queue_damage(g, owner, player_target, m.damage[0]);
            break;
        case 3: {
            Effect e{}; e.kind = EffectKind::VampireDamageAction; e.source = owner; e.target = player_target;
            e.info_base = m.damage[2]; e.amount = m.damage_output[2]; add_bottom(g, e); break;
        }
        case 4: set_monster_move(g, owner, 1, Intent::AttackDebuff, m.damage[1]); break;
        }
        queue_monster_roll(g, owner);
    }
};
}
