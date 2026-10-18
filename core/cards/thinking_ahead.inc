#pragma once
#include "core/hooks.hpp"
#include "core/actions/draw.hpp"

namespace spire::units::cards {
struct ThinkingAhead : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 0, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        add_bottom(g, actions::Draw::construct(2, true, -1, player_target));
        if (!g.hand.empty()) {
            Effect e{};
            e.kind = EffectKind::PutOnDeck;
            e.source = e.target = player_target;
            e.amount = 1;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.exhaust = false;
        }
    }
};
}
