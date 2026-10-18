#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Devotion : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::None, 1, -1, -1, 2, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect sound{};
        sound.kind = EffectKind::SFX;
        sound.source = sound.target = no_target;
        add_bottom(g, sound);
        Effect visual{};
        visual.kind = EffectKind::VFX;
        visual.source = visual.target = no_target;
        add_bottom(g, visual);
        queue_power(g, player_target, player_target, PowerId::Devotion, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
