#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct FlechetteAction : ActionHooks {
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
        for (int i = 0; i < g.hand.size; ++i) {
            if (card_data(g.cards[g.hand[i]].id).type != CardType::Skill) continue;
            Effect hit{};
            hit.kind = EffectKind::Damage;
            hit.source = e.source;
            hit.target = e.target;
            hit.amount = e.amount;
            hit.info_base = e.info_base;
            hit.damage_type = e.damage_type;
            hit.flag = true;
            add_top(g, hit);
        }
        e.done = true;
    }
};
}
