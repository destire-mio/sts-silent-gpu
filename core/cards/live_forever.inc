#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct LiveForever : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Power, CardTarget::None, -2, -1, -1, 6, 0, 0, 2, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) { onChoseThisOption(g, card); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 2;
            card.magic = card.base_magic;
        }
    }
    static SPIRE_FN void onChoseThisOption(Game& g, int card) {
        {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = no_target;
            e.target = no_target;
            add_bottom(g, e);
        }
        {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = e.target = player_target;
            add_bottom(g, e);
        }
        queue_power(g, player_target, player_target, PowerId::PlatedArmor, g.cards[card].magic, false, 0);
    }
};
}
