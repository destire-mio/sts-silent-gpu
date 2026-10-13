#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct EndTurnDiscard : ActionHooks {
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        for (int i = 0; i < g.hand.size;) {
            const Card& card = g.cards[g.hand[i]];
            if (card.retain || card.self_retain) g.retained.push_back(g.hand.erase(i));
            else ++i;
        }
        Effect restore{};
        restore.kind = EffectKind::RestoreRetained;
        add_top(g, restore);
        if (!has_relic(g, "Runic Pyramid") && find_power(g, g.player.creature, "Equilibrium") < 0) {
            const int count = g.hand.size;
            for (int i = 0; i < count; ++i) {
                Effect discard{};
                discard.kind = EffectKind::Discard;
                discard.source = no_target;
                discard.amount = g.hand.size;
                discard.flag = true;
                discard.extra = 1;
                add_top(g, discard);
            }
        }
        Pile shuffled = g.hand;
        shuffle_unseeded(shuffled.values, shuffled.size, g.unseeded);
        for (int i = 0; i < shuffled.size; ++i) card_triggerOnEndOfPlayerTurn(g, shuffled[i]);
        e.done = true;
    }
};
}
