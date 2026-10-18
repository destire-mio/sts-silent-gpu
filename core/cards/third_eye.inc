#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct ThirdEye : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 1, -1, 7, 3, 0, 2, 2, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = no_target;
            e.target = no_target;
            add_bottom(g, e);
        }
        queue_block(g, player_target, g.cards[card].block);
        {
            Effect e{};
            e.kind = EffectKind::Scry;
            action_initialize_state(e);
            e.source = e.target = no_target;
            e.amount = g.cards[card].magic;
            e.extra = 0;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 2;
            card.magic = card.base_magic;
        }
    }
};
}
