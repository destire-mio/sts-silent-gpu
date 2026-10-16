#pragma once
#include "../hooks.hpp"
#include "show_card_and_poof.hpp"
#include "show_card.hpp"

namespace spire::units::actions {
struct UseCard : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.actionType", scalar, effect.flag ? "EXHAUST" : "USE")
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 1)
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", player)
    SPIRE_SNAPSHOT("UseCardAction.targetCard", card, effect.card)
    SPIRE_SNAPSHOT("UseCardAction.exhaustCard", scalar, effect.flag)
    SPIRE_SNAPSHOT("UseCardAction.target", creature, effect.target < 0 ? no_target : effect.target)
    SPIRE_STATE(UseCard, bool rebound_card; bool return_to_hand;)
    static SPIRE_FN constexpr ActionData data() { return {true}; }
    SPIRE_FN static Effect construct(Game& g, int card_id, int target) {
        Effect e{};
        e.kind = EffectKind::UseCard;
        e.card = card_id;
        e.target = target;
        action_initialize_state(e);
        e.amount = 1;
        e.source = no_target;
        Card& card = g.cards[e.card];
        e.flag = card.exhaust_on_use_once || card.exhaust;
        Powers& powers = g.player.creature.powers;
        for (int p = next_power_onUseCard(g, powers.head); p != 255; p = next_power_onUseCard(g, g.power_objects[p].next))
            if (!card.dont_trigger_on_use_card) power_onUseCard(g, g.power_objects[p], player_target, e.card, e);
        for (int i = next_relic_onUseCard(g, 0); i < g.run.relics.size; i = next_relic_onUseCard(g, i + 1))
            if (!card.dont_trigger_on_use_card) relic_onUseCard(g, i, e.card, e);
        for (int i = next_card_triggerOnCardPlayed(g, g.hand, 0); i < g.hand.size; i = next_card_triggerOnCardPlayed(g, g.hand, i + 1))
            if (!card.dont_trigger_on_use_card) card_triggerOnCardPlayed(g, g.hand[i], e.card);
        for (int i = next_card_triggerOnCardPlayed(g, g.discard, 0); i < g.discard.size; i = next_card_triggerOnCardPlayed(g, g.discard, i + 1))
            if (!card.dont_trigger_on_use_card) card_triggerOnCardPlayed(g, g.discard[i], e.card);
        for (int i = next_card_triggerOnCardPlayed(g, g.draw, 0); i < g.draw.size; i = next_card_triggerOnCardPlayed(g, g.draw, i + 1))
            if (!card.dont_trigger_on_use_card) card_triggerOnCardPlayed(g, g.draw[i], e.card);
        for (int m = 0; m < g.monster_order.size; ++m) {
            const int owner = g.monster_order[m];
            Powers& other = g.monsters[owner].creature.powers;
            for (int p = next_power_onUseCard(g, other.head); p != 255; p = next_power_onUseCard(g, g.power_objects[p].next))
                if (!card.dont_trigger_on_use_card) power_onUseCard(g, g.power_objects[p], owner, e.card, e);
        }
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        Card& card = g.cards[e.card];
        Powers& powers = g.player.creature.powers;
        for (int p = next_power_onAfterUseCard(g, powers.head); p != 255; p = next_power_onAfterUseCard(g, g.power_objects[p].next))
            if (!card.dont_trigger_on_use_card) power_onAfterUseCard(g, g.power_objects[p], player_target, e.card, e);
        for (int m = 0; m < g.monster_order.size; ++m) {
            const int owner = g.monster_order[m];
            Powers& other = g.monsters[owner].creature.powers;
            for (int p = next_power_onAfterUseCard(g, other.head); p != 255; p = next_power_onAfterUseCard(g, g.power_objects[p].next))
                if (!card.dont_trigger_on_use_card) power_onAfterUseCard(g, g.power_objects[p], owner, e.card, e);
        }
        card.free_to_play_once = false;
        card.in_autoplay = false;
        if (card.purge_on_use) {
            add_top(g, ShowCardAndPoof::construct(e.card));
            g.player.card_in_use = -1;
            e.done = true;
            return;
        }
        if (card_data(card.id).type == CardType::Power) {
            add_top(g, ShowCard::construct(e.card));
            remove_queued_card(g, e.card);
            for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
            g.player.card_in_use = -1;
            e.done = true;
            return;
        }
        g.player.card_in_use = -1;
        bool spoon = false;
        bool exhaust_discard = false;
        for (int i = 0; i < g.run.relics.size; ++i) exhaust_discard |= relic_data(g.run.relics[i].id).exhaust_discard;
        if (e.flag && exhaust_discard) spoon = g.rng.card_random.randomBoolean();
        if (!e.flag || spoon) {
            if (e.payload.state.UseCard.rebound_card) {
                remove_queued_card(g, e.card);
                card_resetAttributes(g, e.card);
                g.draw.push_back(e.card);
            } else if (card.shuffle_back_into_draw_pile) {
                remove_queued_card(g, e.card);
                card_resetAttributes(g, e.card);
                if (g.draw.empty()) g.draw.push_back(e.card);
                else g.draw.insert(g.rng.card_random.random(g.draw.size - 1), e.card);
            } else if (card.return_to_hand) {
                // UseCardAction.update reads targetCard.returnToHand, not its own returnToHand field.
                remove_queued_card(g, e.card);
                g.hand.push_back(e.card);
                for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
                on_draw_or_discard(g);
            } else {
                move_to_discard(g, e.card);
            }
        } else {
            exhaust_card(g, e.card);
        }
        card.exhaust_on_use_once = false;
        card.dont_trigger_on_use_card = false;
        Effect check{};
        check.kind = EffectKind::HandCheck;
        add_bottom(g, check);
        e.done = true;
    }
};
}
