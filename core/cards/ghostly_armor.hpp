#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct GhostlyArmor : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 10, -1, 0, 3, 0, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void triggerOnEndOfPlayerTurn(Game& g, int) {
        Effect e{};
        e.kind = EffectKind::ExhaustAllEthereal;
        e.source = e.target = no_target;
        add_top(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) ++card.upgrades;
    }
};
}
