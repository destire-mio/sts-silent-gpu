#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct SnakeDagger : MonsterHooks {
    SPIRE_STATE(SnakeDagger, bool firstMove;)
    SPIRE_FN static constexpr MonsterData data() { return {20, 25, {9, 25, 25}, 0, 0, 3, EnemyType::Normal, {DamageType::Normal, DamageType::Normal, DamageType::HpLoss}}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        Monster m = make_monster(g, MonsterId::SnakeDagger, data(), hp_override);
        m.state.SnakeDagger.firstMove = true;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) {
        auto& s = g.monsters[owner].state.SnakeDagger;
        if (s.firstMove) { s.firstMove = false; set_monster_move(g, owner, 1, Intent::AttackDebuff, 9); }
        else set_monster_move(g, owner, 2, Intent::Attack, 25);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        add_bottom(g, actions::ChangeState::make(g, owner, m.move == 1 ? "ATTACK" : "SUICIDE"));
        queue_damage(g, owner, player_target, m.damage[m.move == 1 ? 0 : 1]);
        if (m.move == 1) queue_monster_card(g, CardId::Wound, EffectKind::AddCardDiscard, 1);
        else { Effect e{}; e.kind = EffectKind::LoseHP; e.source = e.target = owner; e.amount = m.creature.hp; add_bottom(g, e); }
        queue_monster_roll(g, owner);
    }
};
}
