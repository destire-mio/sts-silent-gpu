#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct BlockPerNonAttack : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "BLOCK"}; }
    SPIRE_SNAPSHOT("BlockPerNonAttackAction.blockPerCard", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        Pile cards{};
        for (int i = 0; i < g.hand.size; ++i)
            if (card_data(g.cards[g.hand[i]].id).type != CardType::Attack)
                cards.push_back(g.hand[i]);
        for (int i = 0; i < cards.size; ++i) {
            Effect a{};
            a.kind = EffectKind::Block;
            a.amount = e.extra;
            add_top(g, a);
        }
        for (int i = 0; i < cards.size; ++i) {
            Effect a{};
            a.kind = EffectKind::ExhaustCard;
            action_initialize_state(a);
            a.card = cards[i];
            a.source = a.target = no_target;
            add_top(g, a);
        }
    }
};
}
