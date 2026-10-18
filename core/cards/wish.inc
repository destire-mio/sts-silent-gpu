#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Wish : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::None, 3, 3, 6, 25, 1, 2, 5, true, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::ChooseOne;
        action_initialize_state(e);
        e.source = e.target = no_target;
        const CardId ids[] = {CardId::BecomeAlmighty, CardId::FameAndFortune, CardId::LiveForever};
        for (int i = 0; i < 3; ++i)
            e.payload.state.ChooseOne.choices[i] = create_card(g, make_card(ids[i], g.cards[card].upgrades));
        e.payload.state.ChooseOne.count = 3;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 1;
            card.base_magic += 5;
            card.magic = card.base_magic;
        }
    }
    static SPIRE_FN void applyPowers(Game &, int) {}
    static SPIRE_FN void calculateCardDamage(Game &, int, int) {}
};
}
