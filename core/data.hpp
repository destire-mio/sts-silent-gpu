#pragma once
#include "state.hpp"

namespace spire {
struct CardDef {
    CardType type;
    CardTarget target;
    int cost, damage, block, magic;
    int upgrade_damage, upgrade_block, upgrade_magic;
    bool exhaust, ethereal;
};
struct PowerData {
    int priority = 5;
    bool debuff = false;
    bool can_negative = false;
    bool can_stack = true;
    bool blocks_draw = false;
    bool prevents_attacks = false;
    bool free_attacks = false;
};
struct RelicData { int counter = -1; int price = 300; bool can_play_status = false; bool can_play_curse = false; };
struct PotionData { int potency = 0; CardTarget target = CardTarget::Self; bool usable = true; };
struct ActionData { bool survives_combat_end = false; };
struct MonsterData { int hp_min, hp_max; int damage[2]; int block, magic; };
}
