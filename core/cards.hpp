#pragma once
#include "combat.hpp"

namespace spire {
SPIRE_FN inline bool can_play(const Game& g, const Card& card) {
    if (card.cost_for_turn == -2) return false;
    if (card_def(card.id).type == CardType::Attack && has_power(g.player.creature, PowerId::Entangled)) return false;
    return card.free_to_play_once || card.cost_for_turn <= g.player.energy;
}
SPIRE_FN inline void use_card(Game& g, int card_id, int target) {
    Card& card = g.cards[card_id];
    int damage_target = target;
    // AbstractCard.calculateCardDamage(null) computes a value for each monster, including skills.
    if (target == no_target && !g.monster_order.empty()) {
        const int first = g.monster_order[0];
        if (!g.monsters[first].creature.dying && !g.monsters[first].creature.escaped) damage_target = first;
    }
    card.damage = calculate_damage(g, player_target, damage_target, card.base_damage);
    card.block = calculate_block(g, card.base_block);
    switch (card.id) {
        case CardId::Strike_G:
            queue_damage(g, player_target, target, card.base_damage);
            break;
        case CardId::Neutralize:
            queue_damage(g, player_target, target, card.base_damage);
            queue_power(g, player_target, target, PowerId::Weak, card.magic);
            break;
        case CardId::Defend_G:
            queue_block(g, player_target, card.block);
            break;
        case CardId::Survivor: {
            queue_block(g, player_target, card.block);
            Effect e{};
            e.kind = EffectKind::Discard;
            e.amount = 1;
            add_bottom(g, e);
            break;
        }
        case CardId::Slimed: break;
        default: SPIRE_ASSERT(false);
    }
    Effect finish{};
    finish.kind = EffectKind::UseCard;
    finish.card = card_id;
    finish.target = target;
    finish.flag = card.exhaust || card.exhaust_on_use_once;
    add_bottom(g, finish);
    int hand_index = 0;
    while (g.hand[hand_index] != card_id) ++hand_index;
    g.hand.erase(hand_index);
    g.player.card_in_use = card_id;
    if (!card.free_to_play_once && card.cost_for_turn > 0) g.player.energy -= card.cost_for_turn;
}
SPIRE_FN inline void starter_deck(Run& run) {
    run.master_deck.clear();
    for (int i = 0; i < 5; ++i) run.master_deck.push_back(make_card(CardId::Strike_G));
    for (int i = 0; i < 5; ++i) run.master_deck.push_back(make_card(CardId::Defend_G));
    run.master_deck.push_back(make_card(CardId::Survivor));
    run.master_deck.push_back(make_card(CardId::Neutralize));
    run.master_deck.push_back(make_card(CardId::AscendersBane));
    run.relics.push_back({RelicId::SnakeRing, -1});
}
SPIRE_FN inline void relics_at_battle_start(Game& g) {
    for (int i = 0; i < g.run.relics.size; ++i) {
        switch (g.run.relics[i].id) {
            case RelicId::SnakeRing: {
                Effect e{};
                e.kind = EffectKind::Draw;
                e.amount = 2;
                add_bottom(g, e);
                break;
            }
            default: SPIRE_ASSERT(false);
        }
    }
}
}
