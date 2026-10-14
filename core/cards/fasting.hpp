#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Fasting : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::Self, 2, -1, -1, 3, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect visual{};
        visual.kind = EffectKind::VFX;
        visual.source = visual.target = no_target;
        add_bottom(g, visual);
        queue_power(g, player_target, player_target, PowerId::Strength, g.cards[card].magic);
        queue_power(g, player_target, player_target, PowerId::Dexterity, g.cards[card].magic);
        queue_power(g, player_target, player_target, PowerId::EnergyDown, 1);
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
