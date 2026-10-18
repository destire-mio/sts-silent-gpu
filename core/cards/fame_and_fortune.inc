#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct FameAndFortune : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::None, -2, -1, -1, 25, 0, 0, 5, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) { onChoseThisOption(g, card); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 5;
            card.magic = card.base_magic;
        }
    }
    static SPIRE_FN void onChoseThisOption(Game& g, int card) {
        {
            Effect e{};
            e.kind = EffectKind::GainGold;
            action_initialize_state(e);
            e.source = e.target = no_target;
            e.amount = g.cards[card].magic;
            e.extra = 0;
            add_bottom(g, e);
        }
    }
};
}
