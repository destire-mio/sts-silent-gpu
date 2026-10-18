#pragma once
#include "core/hooks.hpp"
#include "core/actions/exhaust_card.hpp"

namespace spire::units::cards {
struct DeusExMachina : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, -2, -1, -1, 2, 0, 0, 1, true, false};
        return d;
    }
    static SPIRE_FN void use(Game&, int, int) {}
    static SPIRE_FN bool canUse(const Game&, int, int) { return false; }
    static SPIRE_FN void triggerWhenDrawn(Game& g, int card) {
        Effect e{};
        e.kind = EffectKind::MakeTempCardInHand;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.card = create_card(g, make_card(CardId::Miracle, 0));
        e.amount = g.cards[card].magic;
        add_top(g, e);
        add_top(g, actions::ExhaustCard::make(card, PileId::Hand));
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
