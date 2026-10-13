#pragma once
#include "state.hpp"

namespace spire {
struct CardDef {
    CardType type;
    CardTarget target;
    int cost;
    int damage;
    int block;
    int magic;
    int upgrade_damage;
    int upgrade_block;
    int upgrade_magic;
    bool exhaust;
    bool ethereal;
};
SPIRE_FN inline CardDef card_def(CardId id) {
#define SPIRE_CARD_DATA(id, name, type, target, cost, damage, block, magic, up_damage, up_block, up_magic, exhaust, ethereal) \
    {CardType::type, CardTarget::target, cost, damage, block, magic, up_damage, up_block, up_magic, exhaust, ethereal},
    constexpr CardDef definitions[] = {
        SPIRE_CARD_CATALOG(SPIRE_CARD_DATA)
    };
#undef SPIRE_CARD_DATA
    static_assert(sizeof(definitions) / sizeof(CardDef) == static_cast<int>(CardId::Count));
    SPIRE_ASSERT(id < CardId::Count);
    return definitions[static_cast<int>(id)];
}
SPIRE_FN inline void reset_attributes(Card& card) {
    card.damage = card.base_damage;
    card.block = card.base_block;
    card.magic = card.base_magic;
    card.cost_for_turn = card.cost;
}
SPIRE_FN inline Card make_card(CardId id, int upgrades = 0, int misc = 0) {
    const CardDef def = card_def(id);
    SPIRE_ASSERT(upgrades == 0 || (upgrades == 1 && (def.type == CardType::Attack || def.type == CardType::Skill)));
    Card card{};
    card.id = id;
    card.upgrades = upgrades;
    card.misc = misc;
    card.cost = def.cost;
    card.base_damage = def.damage + upgrades * def.upgrade_damage;
    card.base_block = def.block + upgrades * def.upgrade_block;
    card.base_magic = def.magic + upgrades * def.upgrade_magic;
    card.exhaust = def.exhaust;
    card.ethereal = def.ethereal;
    card.cost_for_turn = card.cost;
    card.magic = card.base_magic;
    return card;
}
}
