#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Foresight : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::None, 1, -1, -1, 3, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        for (int i = 0; i < 2; ++i) {
            Effect visual{};
            visual.kind = EffectKind::VFX;
            visual.source = visual.target = no_target;
            add_bottom(g, visual);
        }
        queue_power(g, player_target, player_target, PowerId::Foresight, g.cards[card].magic);
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
