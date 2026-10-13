#pragma once
#include "state.hpp"

namespace spire {
struct CardDef {
    CardType type;
    CardTarget target;
    int cost, damage, block, magic;
    int upgrade_damage, upgrade_block, upgrade_magic;
    bool exhaust, ethereal;
    bool innate = false;
    bool multi_damage = false;
    int misc = 0;
    bool strike = false;
};
struct PowerData {
    int priority = 5;
    bool debuff = false;
    bool can_negative = false;
    bool can_stack = true;
    bool blocks_draw = false;
    bool prevents_attacks = false;
    bool free_attacks = false;
    bool turn_based = false;
    bool stacks_separately = false;
    bool post_action = false;
    bool keep_player_block = false;
    bool keep_monster_block = false;
    bool blocks_debuffs = false;
    int incoming_damage_limit = 0;
    float rear_damage_multiplier = 1.0f;
    PowerId rear_attack_power = PowerId::Count;
};
struct RelicData {
    int counter = -1;
    int price = 300;
    bool can_play_status = false;
    bool can_play_curse = false;
    bool keep_hand = false;
    bool exhaust_discard = false;
    bool blocks_potions = false;
    int poison_bonus = 0;
    int rest_heal_bonus = 0;
    int bowl_hp = 0;
    int purge_cost = -1;
    bool any_color_rewards = false;
    bool hides_intents = false;
    int score = 0;
    const char* empty_orb_class = nullptr;
};
struct PotionData { int potency = 0; CardTarget target = CardTarget::Self; bool usable = true; bool revives_player = false; };
struct ActionData { bool survives_combat_end = false; const char* type = nullptr; bool multi_step = false; };
struct MonsterData { int hp_min, hp_max; int damage[2]; int block, magic; uint8_t damage_count = 1; EnemyType type = EnemyType::Normal; };
}
