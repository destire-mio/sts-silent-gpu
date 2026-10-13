#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Regret : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Curse, CardTarget::None, -2, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        if (g.cards[card].dont_trigger_on_use_card) {
            Effect e{};
            e.kind = EffectKind::LoseHP;
            e.source = e.target = player_target;
            e.amount = g.cards[card].magic;
            add_top(g, e);
        }
    }
    static SPIRE_FN void triggerOnEndOfTurnForPlayingCard(Game& g, int card) {
        g.cards[card].dont_trigger_on_use_card = true;
        g.cards[card].magic = g.cards[card].base_magic = g.hand.size;
        CardQueueItem item{};
        item.card = card;
        item.energy_on_use = g.player.energy;
        item.is_end_turn_auto_play = true;
        g.manager.card_queue.push_back(item);
    }
    static SPIRE_FN void upgrade(Card&) {}
};
}
