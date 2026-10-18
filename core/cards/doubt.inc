#pragma once
#include "core/hooks.hpp"
#include "core/actions/apply_power.hpp"

namespace spire::units::cards {
struct Doubt : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Curse, CardTarget::None, -2, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        if (g.cards[card].dont_trigger_on_use_card) {
            Power p{PowerId::Weak, 1};
            power_constructPower(g, p, player_target, player_target, 0, -1);
            power_initFromSource(g, p, player_target, true);
            add_top(g, actions::ApplyPower::make(player_target, player_target, p, 1));
        }
    }
    static SPIRE_FN void upgrade(Card&) {}
    static SPIRE_FN void triggerOnEndOfTurnForPlayingCard(Game& g, int card) {
        g.cards[card].dont_trigger_on_use_card = true;
        CardQueueItem item{};
        item.card = card;
        item.energy_on_use = g.player.energy;
        item.is_end_turn_auto_play = true;
        g.manager.card_queue.push_back(item);
    }
    static SPIRE_FN void triggerWhenDrawn(Game& g, int card) {
        Effect e{};
        e.kind = EffectKind::SetDontTrigger;
        e.source = e.target = no_target;
        e.card = card;
        add_bottom(g, e);
    }
};
}
