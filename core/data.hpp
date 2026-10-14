#pragma once
#include "state.hpp"

namespace spire {
// Cutscene.update keeps subtracting until the panels finish, then resets their finished flags.
SPIRE_FN constexpr float victory_switch_timer() {
    float timer = 5.0f;
    while (timer >= 0.0f) timer -= 1.0f / 60.0f;
    float alpha = 1.0f;
    while (alpha >= 0.0f) { alpha -= 1.0f / 60.0f; timer -= 1.0f / 60.0f; }
    return timer;
}
struct EventData { const char* java_class; bool image; bool reward_opens_map = false; const char* control_name = nullptr; const char* combat_metric = nullptr; };
struct CardDef {
    CardType type;
    CardTarget target;
    int cost, damage, block, magic;
    int upgrade_damage, upgrade_block, upgrade_magic;
    bool exhaust, ethereal;
    bool innate = false;
    bool multi_damage = false;
    int misc = 0;
    bool purgeable = true;
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
    bool blocks_gold = false;
    PowerId blocked_debuff = PowerId::Count;
    float gold_reward_bonus = 0.0f;
    bool preserves_energy = false;
    bool revives_player = false;
    bool triggers_at_battle_end = false;
    bool prevents_event_combat = false;
    int event_chest_interval = 0;
    bool screenless_excluded = false;
    bool boss_chest_excluded = false;
    bool replaces_starter = false;
    bool crosses_map_paths = false;
    float shop_multiplier = 1.0f;
    bool restocks_shop = false;
    bool excluded_from_restock = false;
    bool discounts_existing_stock = false;
    float player_vulnerable_multiplier = 0.0f;
    float monster_weak_multiplier = 0.0f;
    bool extra_normal_combat_card_reward = false;
    bool extra_elite_relic = false;
    bool adds_campfire_option = false;
    bool rest_card_reward = false;
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
    bool prevents_revival = false;
    int potion_reward_chance = -1;
};
struct PotionData { int potency = 0; CardTarget target = CardTarget::Self; bool usable = true; bool revives_player = false; bool limited_exclusion = false; };
struct ActionData { bool survives_combat_end = false; const char* type = nullptr; bool multi_step = false; };
struct MonsterData { int hp_min, hp_max; int damage[8]; int block, magic; uint8_t damage_count = 1; EnemyType type = EnemyType::Normal; DamageType damage_types[8]{}; };
}
