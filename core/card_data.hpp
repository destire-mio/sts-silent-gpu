#pragma once
#include "spire_generated/content.hpp"

namespace spire {
SPIRE_FN inline const CardDef& card_def(CardId id) { return card_data(id); }
SPIRE_FN inline void modify_cost_for_combat(Card& card, int amount) {
    if (card.cost_for_turn > 0) { card.cost_for_turn = max(0, card.cost_for_turn + amount); card.cost = card.cost_for_turn; }
    else if (card.cost >= 0) { card.cost = max(0, card.cost + amount); card.cost_for_turn = 0; }
}
SPIRE_FN inline void update_card_cost(Card& card, int amount) {
    const CardType type = card_data(card.id).type;
    if ((type == CardType::Curse && !card_is(card.id, "Pride")) ||
        (type == CardType::Status && !card_is(card.id, "Slimed"))) return;
    const int cost = max(0, card.cost + amount);
    if (cost != card.cost) {
        const int diff = card.cost - card.cost_for_turn;
        card.cost = cost;
        card.cost_for_turn = max(0, cost - diff);
    }
}
SPIRE_FN inline void upgrade_card_base_cost(Card& card, int cost) {
    const int diff = card.cost_for_turn - card.cost;
    card.cost = cost;
    if (card.cost_for_turn > 0) card.cost_for_turn = card.cost + diff;
    card.cost_for_turn = max(0, card.cost_for_turn);
    card.upgraded_cost = true;
}
SPIRE_FN inline void upgrade_card_damage(Card& card, int amount) {
    card.base_damage += amount;
    card.upgraded_damage = true;
}
SPIRE_FN inline void reset_attributes(Card& card) {
    card.damage = card.base_damage;
    card.block = card_baseBlock(card);
    card.magic = card.base_magic;
    card.cost_for_turn = card.cost;
}
SPIRE_FN inline Card make_card(CardId id, int upgrades) {
    const CardDef def = card_def(id);
    Card card{};
    card.id = id;
    card_initialize_state(card);
    card.misc = def.misc;
    card.target = def.target;
    card.innate = def.innate;
    card.cost = def.cost;
    card.base_damage = def.damage;
    card.base_magic = def.magic;
    card.self_retain = def.self_retain;
    card.block = def.initial_block;
    card.exhaust = def.exhaust;
    card.ethereal = def.ethereal;
    for (int i = 0; i < upgrades; ++i) card_upgrade(card);
    card.cost_for_turn = card.cost;
    card.magic = card.base_magic;
    return card;
}
SPIRE_FN inline Card make_card(CardId id, int upgrades, int misc) {
    Card card = make_card(id, upgrades);
    card.misc = misc;
    return card;
}
}
