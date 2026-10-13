#pragma once
#include "spire_generated/content.hpp"

namespace spire {
SPIRE_FN inline CardDef card_def(CardId id) { return card_data(id); }
SPIRE_FN inline void reset_attributes(Card& card) {
    card.damage = card.base_damage;
    card.block = card.base_block;
    card.magic = card.base_magic;
    card.cost_for_turn = card.cost;
}
SPIRE_FN inline Card make_card(CardId id, int upgrades, int misc) {
    const CardDef def = card_def(id);
    Card card{};
    card.id = id;
    card_initialize_state(card);
    card.misc = misc;
    card.cost = def.cost;
    card.base_damage = def.damage;
    card.base_block = def.block;
    card.base_magic = def.magic;
    card.exhaust = def.exhaust;
    card.ethereal = def.ethereal;
    for (int i = 0; i < upgrades; ++i) card_upgrade(card);
    card.cost_for_turn = card.cost;
    card.magic = card.base_magic;
    return card;
}
}
