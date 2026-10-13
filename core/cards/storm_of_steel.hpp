#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct StormOfSteel : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect shivs{};
        shivs.kind = EffectKind::BladeFuryAction;
        shivs.flag = g.cards[card].upgrades != 0;
        add_bottom(g, shivs);
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::StormOfSteel); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
