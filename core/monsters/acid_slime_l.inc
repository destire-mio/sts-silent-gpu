#pragma once
#include "core/monsters/base.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::monsters {
struct AcidSlime_L : MonsterHooks {
    SPIRE_STATE(AcidSlime_L, bool splitTriggered;)
    SPIRE_SNAPSHOT("AcidSlime_L.saveX", scalar, monster.offset_x)
    SPIRE_FN static constexpr MonsterData data() { return {68, 72, {12, 18}, 0, 2, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 65; stats.hp_max = 69; }
        if (g.run.ascension < 2) { stats.damage[0] = 11; stats.damage[1] = 16; }
        Monster m = make_monster(g, MonsterId::AcidSlime_L, stats, hp_override);
        Power split{PowerId::Split, -1};
        split.instance = next_power_identity(g);
        power_initialize_state(split);
        m.creature.powers.push_back(g.power_objects, g.power_objects.add(split));
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if (num < (g.run.ascension >= 17 ? 40 : 30)) {
            if (!last_two_moves(g, m, 1)) set_monster_move(g, index, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0));
            else {
                const bool attack = g.run.ascension >= 17 ? g.rng.ai.randomBoolean(0.6f) : g.rng.ai.randomBoolean();
                if (attack) set_monster_move(g, index, 2, Intent::Attack, monster_base_damage(g, m, 1));
                else set_monster_move(g, index, 4, Intent::Debuff);
            }
        } else if (num < 70) {
            if (!(g.run.ascension >= 17 ? last_two_moves(g, m, 2) : last_move(g, m, 2))) set_monster_move(g, index, 2, Intent::Attack, monster_base_damage(g, m, 1));
            else if (g.rng.ai.randomBoolean(g.run.ascension >= 17 ? 0.6f : 0.4f))
                set_monster_move(g, index, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0));
            else set_monster_move(g, index, 4, Intent::Debuff);
        } else if (!(g.run.ascension >= 17 ? last_move(g, m, 4) : last_two_moves(g, m, 4))) set_monster_move(g, index, 4, Intent::Debuff);
        else if (g.rng.ai.randomBoolean(0.4f)) set_monster_move(g, index, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0));
        else set_monster_move(g, index, 2, Intent::Attack, monster_base_damage(g, m, 1));
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 3) {
            queue_split(g, index, MonsterId::AcidSlime_M, MonsterId::AcidSlime_M, m.offset_x - 134.0f, m.offset_x + 134.0f);
            set_monster_move(g, index, 3, Intent::Unknown);
            return;
        }
        if (m.move == 4) queue_power(g, index, player_target, PowerId::Weak, data().magic, true);
        else {
            queue_monster_damage(g, index, player_target, m.move == 2 ? 1 : 0);
            if (m.move == 1) queue_slimed(g, 2);
        }
        queue_monster_roll(g, index);
    }
    SPIRE_FN static void damage(Game& g, int index, const DamageInfo& info) {
        monster_damage_base(g, index, info);
        onDamage(g, index);
    }
    SPIRE_FN static void onDamage(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (!m.creature.dying && static_cast<float>(m.creature.hp) <= static_cast<float>(m.creature.max_hp) / 2.0f &&
            m.move != 3 && !m.state.AcidSlime_L.splitTriggered) {
            set_monster_move(g, index, 3, Intent::Unknown);
            monster_createIntent(g, index);
            add_bottom(g, actions::TextAboveCreature::make(index));
            queue_monster_move(g, index, 3, Intent::Unknown);
            m.state.AcidSlime_L.splitTriggered = true;
        }
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        monster_die_base(g, owner, trigger_relics);
        for (int i = 0; i < g.manager.actions.size; ++i)
            if (g.action_objects[g.manager.actions[i]].kind == EffectKind::SpawnMonster) return;
        if (monsters_basically_dead(g) && g.room_kind == RoomKind::Boss) monster_onBossVictoryLogic(g, owner);
    }
};
}
