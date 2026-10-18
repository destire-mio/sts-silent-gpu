#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct PressurePoints : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Enemy, 1, -1, -1, 8, 0, 0, 3, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (target != no_target) {
            Effect v{};
            v.kind = EffectKind::VFX;
            v.source = v.target = no_target;
            add_bottom(g, v);
        }
        queue_power(g, player_target, target, PowerId::Mark, g.cards[card].magic);
        Effect e{};
        e.kind = EffectKind::TriggerMarks;
        e.source = e.target = no_target;
        e.card = card;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 3;
            card.magic = card.base_magic;
        }
    }
};
}
