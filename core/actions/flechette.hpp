#pragma once
#include "../hooks.hpp"
#include "damage.hpp"

namespace spire::units::actions {
struct FlechetteAction : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("FlechetteAction.info", damage_info, effect, queue, index)
    static SPIRE_FN constexpr ActionData data() { return {false, "BLOCK"}; }
    SPIRE_FN static Effect construct(int target, const DamageInfo& info) {
        Effect e{};
        e.kind = EffectKind::FlechetteAction;
        action_initialize_state(e);
        e.target = target;
        e.source = info.source;
        e.amount = info.amount;
        e.info_base = info.info_base;
        e.damage_type = info.damage_type;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        const uint32_t identity = Damage::info_identity(g);
        for (int i = 0; i < g.hand.size; ++i) {
            if (card_data(g.cards[g.hand[i]].id).type != CardType::Skill) continue;
            add_top(g, Damage::from_shared_info(e, e.target, identity, true));
            if (e.target != no_target) {
                Effect visual{};
                visual.kind = EffectKind::VFX;
                visual.source = visual.target = no_target;
                add_top(g, visual);
            }
        }
        e.done = true;
    }
};
}
