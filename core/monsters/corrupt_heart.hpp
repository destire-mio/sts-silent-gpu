#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct CorruptHeart : MonsterHooks {
    SPIRE_STATE(CorruptHeart, int bloodHitCount; int moveCount; int buffCount; bool isFirstMove;)
    SPIRE_FN static constexpr MonsterData data() { return {800, 800, {45, 2}, 0, 0, 2, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data(); if (g.run.ascension < 4) d.damage[0] = 40;
        d.hp_min = d.hp_max = g.run.ascension >= 9 ? 800 : 750;
        Monster m = make_monster(g, MonsterId::CorruptHeart, d, hp);
        m.state.CorruptHeart.isFirstMove = true;
        m.state.CorruptHeart.bloodHitCount = g.run.ascension >= 4 ? 15 : 12;
        position_monster(m, -50.0f); return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::Invincible, g.run.ascension >= 19 ? 200 : 300);
        queue_power(g, owner, owner, PowerId::BeatOfDeath, g.run.ascension >= 19 ? 2 : 1);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        (void)num; Monster& m = g.monsters[owner]; auto& s = m.state.CorruptHeart;
        if (s.isFirstMove) { s.isFirstMove = false; set_monster_move(g, owner, 3, Intent::StrongDebuff); return; }
        const int phase = s.moveCount++ % 3;
        if (phase == 2) set_monster_move(g, owner, 4, Intent::Buff);
        else if (phase == 0 ? g.rng.ai.randomBoolean() : last_move(m, 2)) set_monster_move(g, owner, 1, Intent::Attack, m.damage[1], s.bloodHitCount);
        else set_monster_move(g, owner, 2, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner]; auto& s = m.state.CorruptHeart;
        if (m.move == 3) {
            queue_power(g, owner, player_target, PowerId::Vulnerable, 2, true);
            queue_power(g, owner, player_target, PowerId::Weak, 2, true);
            queue_power(g, owner, player_target, PowerId::Frail, 2, true);
            constexpr CardId ids[]{CardId::Dazed, CardId::Slimed, CardId::Wound, CardId::Burn, CardId::VoidCard};
            for (int i = 0; i < 5; ++i) {
                Effect e{}; e.kind = EffectKind::MakeTempCardInDrawPile; action_initialize_state(e); e.card = g.cards.size; e.amount = 1;
                e.state.MakeTempCardInDrawPile.random_spot = true; g.cards.push_back(make_card(ids[i])); g.cards[e.card].uuid = g.next_card_uuid++; add_bottom(g, e);
            }
        } else if (m.move == 4) {
            const int strength = power_amount(g, m.creature, PowerId::Strength);
            queue_power(g, owner, owner, PowerId::Strength, max(0, -strength) + 2);
            switch (s.buffCount++) {
            case 0: queue_power(g, owner, owner, PowerId::Artifact, 2); break;
            case 1: queue_power(g, owner, owner, PowerId::BeatOfDeath, 1); break;
            case 2: queue_power(g, owner, owner, PowerId::PainfulStabs, -1); break;
            case 3: queue_power(g, owner, owner, PowerId::Strength, 10); break;
            default: queue_power(g, owner, owner, PowerId::Strength, 50); break;
            }
        } else if (m.move == 1) for (int i = 0; i < s.bloodHitCount; ++i) queue_damage(g, owner, player_target, m.damage[1]);
        else queue_damage(g, owner, player_target, m.damage[0]);
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        if (!g.cannot_lose) { monster_die_base(g, owner, trigger_relics); monster_onBossVictoryLogic(g, owner); monster_onFinalBossVictoryLogic(g, owner); }
    }
};
}
