#pragma once
#include "core/monsters/base.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Mugger : MonsterHooks {
    SPIRE_STATE(Mugger, int stolenGold; uint8_t slashCount;)
    SPIRE_SNAPSHOT("Mugger.goldAmt", scalar, units::monsters::Mugger::gold_amount(game))
    SPIRE_SNAPSHOT("Mugger.escapeDef", scalar, 11)
    SPIRE_SNAPSHOT("Mugger.swipeDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("Mugger.bigSwipeDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_FN static constexpr MonsterData data() { return {50, 54, {11, 18}, 11, 20, 2}; }
    SPIRE_FN static int gold_amount(const Game& g) { return g.run.ascension < 17 ? 15 : data().magic; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 48; d.hp_max = 52; }
        if (g.run.ascension < 2) { d.damage[0] = 10; d.damage[1] = 16; }
        Monster m = make_monster(g, MonsterId::Mugger, d, hp);
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) { queue_power(g, owner, owner, PowerId::Thievery, units::monsters::Mugger::gold_amount(g)); }
    SPIRE_FN static void getMove(Game& g, int owner, int num) { (void)num; set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, g.monsters[owner], 0)); }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        g.rng.ai.random(2);
        if (g.monsters[owner].state.Mugger.stolenGold > 0) add_stolen_gold_reward(g, g.monsters[owner].state.Mugger.stolenGold);
        monster_die_base(g, owner, trigger_relics);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        auto& s = m.state.Mugger;
        if (m.move == 1 || m.move == 4) {
            const int move = m.move;
            g.rng.ai.random(2);
            if (move == 1 && s.slashCount == 1) g.rng.ai.randomBoolean(0.6f);
            if (move == 4) ++s.slashCount;
            Effect e{}; e.kind = move == 1 ? EffectKind::MuggerStealGold : EffectKind::MuggerBigStealGold; e.target = owner; add_bottom(g, e);
            queue_monster_damage(g, owner, player_target, move == 1 ? 0 : 1, units::monsters::Mugger::gold_amount(g));
            if (move == 4) set_monster_move(g, owner, 2, Intent::Defend);
            else {
                ++s.slashCount;
                if (s.slashCount == 2) {
                    if (g.rng.ai.randomBoolean(0.5f)) set_monster_move(g, owner, 2, Intent::Defend);
                    else queue_monster_move(g, owner, 4, Intent::Attack, monster_base_damage(g, m, 1));
                } else queue_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0));
            }
        } else if (m.move == 2) {
            queue_block(g, owner, g.run.ascension >= 17 ? 17 : 11);
            queue_monster_move(g, owner, 3, Intent::Escape);
        } else {
            add_bottom(g, actions::VFX::make());
            g.mugged = true;
            Effect e{}; e.kind = EffectKind::Escape; e.target = owner; add_bottom(g, e);
            queue_monster_move(g, owner, 3, Intent::Escape);
        }
    }
};
}

namespace spire::units::actions {
struct MuggerStealGold : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("Mugger$1.this$0", creature, effect.target)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        auto& s = g.monsters[e.target].state.Mugger;
        s.stolenGold += min(units::monsters::Mugger::gold_amount(g), g.run.gold);
    }
};
struct MuggerBigStealGold : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("Mugger$2.this$0", creature, effect.target)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        auto& s = g.monsters[e.target].state.Mugger;
        s.stolenGold += min(units::monsters::Mugger::gold_amount(g), g.run.gold);
    }
};
}
