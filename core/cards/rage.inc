#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Rage : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, -1, 3, 0, 0, 2, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::SFX;
        e.source = e.target = no_target;
        add_bottom(g, e);
        e.kind = EffectKind::VFX;
        e.source = e.target = player_target;
        add_bottom(g, e);
        queue_power(g, player_target, player_target, PowerId::Rage, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
