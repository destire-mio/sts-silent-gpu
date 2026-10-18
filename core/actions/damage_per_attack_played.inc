#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage.hpp"

namespace spire::units::actions {
struct DamagePerAttackPlayed : ActionHooks {
    SPIRE_SNAPSHOT("DamagePerAttackPlayedAction.info", damage_info, effect, queue, index)
    SPIRE_FN static constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_FN static Effect construct(int target, const DamageInfo& info) {
        Effect e = info;
        e.kind = EffectKind::DamagePerAttackPlayed;
        action_initialize_state(e);
        e.target = target;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (e.target == no_target || get_creature(g, e.target).hp <= 0) return;
        int count = -1;
        const PlayedCards& played = g.manager.played_cards;
        for (int i = played.turn_start; i < played.size; ++i)
            if (card_def(g.cards[played.values[i]].id).type == CardType::Attack) ++count;
        const uint32_t identity = Damage::info_identity(g);
        for (int i = 0; i < count; ++i) {
            add_top(g, Damage::from_shared_info(e, e.target, identity));
        }
    }
};
}
