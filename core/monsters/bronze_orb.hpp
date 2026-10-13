#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct BronzeOrb : MonsterHooks {
    SPIRE_STATE(BronzeOrb, int count; bool usedStasis;)
    SPIRE_FN static constexpr MonsterData data() { return {54, 60, {8}, 12, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        g.rng.monster_hp.random(52, 58);
        MonsterData stats = data();
        if (g.run.ascension < 9) { stats.hp_min = 52; stats.hp_max = 58; }
        return make_monster(g, MonsterId::BronzeOrb, stats, hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        if (!m.state.BronzeOrb.usedStasis && num >= 25) { set_monster_move(g, owner, 3, Intent::StrongDebuff); m.state.BronzeOrb.usedStasis = true; }
        else if ((num >= 70 && !last_two_moves(g, m, 2)) || last_two_moves(g, m, 1)) set_monster_move(g, owner, 2, Intent::Defend);
        else set_monster_move(g, owner, 1, Intent::Attack, 8);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        if (m.move == 1) {
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e);
            add_bottom(g, actions::VFX::make()); add_bottom(g, actions::VFX::make());
            queue_damage(g, owner, player_target, monster_base_damage(g, m, 0));
        }
        else if (m.move == 2) {
            int target = no_target;
            for (int i = 0; i < g.monster_order.size; ++i) {
                const int index = g.monster_order[i];
                if (monster_is(g.monsters[index].id, "BronzeAutomaton")) { target = index; break; }
            }
            Effect e{}; e.kind = EffectKind::Block; e.source = owner; e.target = target; e.amount = 12; add_bottom(g, e);
        } else { Effect e{}; e.kind = EffectKind::ApplyStasisAction; e.target = owner; add_bottom(g, e); }
        queue_monster_roll(g, owner);
    }
};
}
