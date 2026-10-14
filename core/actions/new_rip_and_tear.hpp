#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct NewRipAndTear : ActionHooks {
    SPIRE_WAITING_RULE("NewRipAndTearAction: waiting for a ported caller with three-seed oracle replay")
    SPIRE_SNAPSHOT("AttackDamageRandomEnemyAction.card", card, effect.card)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        e.target = random_living_monster(g);
        if (e.target != no_target) {
            card_calculateCardDamage(g, e.card, e.target);
            Effect hit{};
            hit.kind = EffectKind::Damage;
            hit.source = player_target;
            hit.target = e.target;
            hit.amount = hit.info_base = g.cards[e.card].damage;
            add_top(g, hit);
        }
        if (e.target != no_target) {
            Effect v{};
            v.source = v.target = no_target;
            v.kind = EffectKind::VFX;
            add_top(g, v);
        }
    }
};
}
