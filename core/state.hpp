#pragma once
#include "fixed.hpp"
#include "random.hpp"
#include "types.hpp"
#include "spire_generated/ids.hpp"

namespace spire {
struct Card {
    uint32_t uuid = 0;
    int misc = 0;
    int base_damage = -1;
    int damage = -1;
    int base_block = -1;
    int block = -1;
    int base_magic = -1;
    int magic = -1;
    CardId id = CardId::Strike_G;
    int16_t upgrades = 0;
    int16_t cost = 0;
    int16_t cost_for_turn = 0;
    int16_t energy_on_use = 0;
    uint8_t multi_damage_count = 0;
    bool ignore_energy_on_use : 1;
    bool exhaust : 1;
    bool ethereal : 1;
    bool retain : 1;
    bool self_retain : 1;
    bool innate : 1;
    bool free_to_play_once : 1;
    bool exhaust_on_use_once : 1;
    bool purge_on_use : 1;
    bool in_autoplay : 1;
    bool dont_trigger_on_use_card : 1;
    CardState state{};
    SPIRE_FN constexpr Card()
        : ignore_energy_on_use(false), exhaust(false), ethereal(false), retain(false),
          self_retain(false), innate(false), free_to_play_once(false), exhaust_on_use_once(false),
          purge_on_use(false), in_autoplay(false), dont_trigger_on_use_card(false) {}
};
using Pile = List<uint16_t, card_capacity>;
struct Relic { RelicId id = RelicId::SnakeRing; int counter = -1; RelicState state{}; };
struct Power {
    uint32_t instance = 0;
    int amount;
    PowerId id;
    int id_offset = -1;
    bool just_applied : 1;
    bool triggered : 1;
    bool skip_first : 1;
    PowerState state{};
    SPIRE_FN constexpr Power(PowerId id = PowerId::Strength, int amount = 0)
        : amount(amount), id(id), just_applied(false), triggered(false), skip_first(true) {}
};
using Powers = List<Power, power_capacity>;
struct Creature {
    float draw_x = 0.0f;
    int hp = 0;
    int max_hp = 0;
    int block = 0;
    int last_damage_taken = 0;
    int gold = 0;
    Powers powers{};
    bool dead = false;
    bool dying = false;
    bool escaped = false;
    bool flip_horizontal = false;
    bool half_dead = false;
};
struct Monster {
    MonsterId id = MonsterId::Cultist;
    Creature creature{};
    float offset_x = 0.0f;
    Intent intent = Intent::Debug;
    int8_t move = -1;
    int16_t intent_base_damage = -1;
    int16_t intent_damage = -1;
    int16_t intent_hits = 0;
    int8_t pending_move = -1;
    Intent pending_intent = Intent::None;
    int16_t pending_base_damage = -1;
    int16_t pending_hits = 0;
    bool pending_multi = false;
    bool intent_multi = false;
    char move_name[128]{};
    bool move_name_present = false;
    List<int8_t, 256> move_history{};
    int16_t damage[3]{};
    uint8_t damage_count = 1;
    int16_t damage_output[3]{};
    bool damage_modified[3]{};
    int16_t block_amount = 0;
    int16_t magic = 0;
    bool split_triggered = false;
    bool escape_next = false;
    MonsterState state{};
};
struct Player {
    Creature creature{};
    int energy = 0;
    int energy_master = 3;
    int energy_per_turn = 3;
    int hand_size = 5;
    int master_hand_size = 5;
    int card_in_use = -1;
    int cards_played_this_turn = 0;
    bool bloodied = false;
};
enum class ActionKind : uint8_t { Play, EndTurn, Choose, PotionUse, PotionDiscard, Proceed, Skip };
struct Action {
    ActionKind kind = ActionKind::EndTurn;
    int index = -1;
    int target = -1;
};
using ActionSet = List<Action, 128>;
struct Effect {
    bool done = false;
    EffectKind kind = EffectKind::Draw;
    int8_t source = player_target;
    int8_t target = player_target;
    int amount = 0;
    int info_base = 0;
    int16_t card = -1;
    PowerId power = PowerId::Strength;
    int extra = 0;
    DamageType damage_type = DamageType::Normal;
    bool flag = false;
    EffectState state{};
    uint32_t power_instance = 0;
    bool end_turn_draw = false;
    Power prepared_power{};
    PotionState prepared_potion_state{};
};
using DamageInfo = Effect;
struct CardQueueItem {
    int card = -1;
    int target = no_target;
    int energy_on_use = 0;
    bool ignore_energy_total = false;
    bool autoplay_card = false;
    bool random_target = false;
    bool is_end_turn_auto_play = false;
};
struct PlayedCards {
    uint16_t values[1024]{};
    uint16_t size = 0;
    uint16_t turn_start = 0;
    uint32_t combat_types[5]{};
    uint32_t turn_types[5]{};
    SPIRE_FN void record(int card, CardType type) {
        SPIRE_ASSERT(size < 1024);
        values[size++] = static_cast<uint16_t>(card);
        ++combat_types[static_cast<int>(type)];
        ++turn_types[static_cast<int>(type)];
    }
    SPIRE_FN void begin_turn() {
        turn_start = size;
        for (int i = 0; i < 5; ++i) turn_types[i] = 0;
    }
    SPIRE_FN void clear() {
        size = 0;
        begin_turn();
        for (int i = 0; i < 5; ++i) combat_types[i] = 0;
    }
};
struct ActionManager {
    Effect current_action{};
    bool has_current_action = false;
    Queue<Effect, action_capacity> actions{};
    Queue<Effect, action_capacity> pre_turn_actions{};
    Queue<CardQueueItem, 32> card_queue{};
    Queue<int, monster_capacity> monster_queue{};
    PlayedCards played_cards{};
    Phase phase = Phase::WaitingOnUser;
    bool turn_has_ended = false;
    bool monster_attacks_queued = true;
    int total_discarded_this_turn = 0;
    int damage_received_this_turn = 0;
    int damage_received_this_combat = 0;
    int hp_loss_this_combat = 0;
    int player_hp_last_turn = 0;
    int energy_gained_this_combat = 0;
};
struct Run {
    uint64_t seed = 0;
    int floor = 0;
    int act = 1;
    int ascension = 20;
    int gold = 99;
    bool ruby_key = false;
    bool emerald_key = false;
    bool sapphire_key = false;
    List<Card, card_capacity> master_deck{};
    List<uint16_t, card_capacity> source_card_pools[4]{};
    List<Relic, 64> relics{};
    PotionId potions[2] = {PotionId::Empty, PotionId::Empty};
    PotionState potion_state[2]{};
};
struct Game {
    Run run{};
    RngStreams rng{};
    UnseededRandom unseeded{0};
    Player player{};
    List<Card, card_capacity> cards{};
    int multi_damage[card_capacity][monster_capacity]{};
    Pile hand{}, draw{}, discard{}, exhaust{}, retained{}, selected_cards{}, limbo{}, grid_cards{};
    List<Effect, action_capacity> action_objects{};
    List<Power, action_capacity> detached_powers{};
    bool player_turn_effect_pending = false;
    Pile drawn_cards{};
    Pile reward_cards{};
    int discovery_card = -1;
    List<Monster, monster_capacity> monsters{};
    List<int, monster_capacity> monster_order{};
    ActionManager manager{};
    Screen screen = Screen::None;
    EncounterId encounter = EncounterId::Cultist;
    EncounterState encounter_state{};
    int turn = 1;
    int discard_remaining = 0;
    int discard_action_num = 0;
    int exhaust_action_num = 0;
    PileId grid_source = PileId::Grid;
    int selection_amount = 0;
    bool selection_any_number = false;
    bool selection_can_pick_zero = false;
    int put_on_deck_num = 0;
    uint32_t next_power_instance = 1;
    int obtained_cards[static_cast<int>(CardId::Count)]{};
    bool selection_for_upgrade = false;
    bool selected_cards_retrieved = false;
    bool end_turn_enabled = true;
    bool selection_up_to = false;
    bool reward_skippable = false;
    bool battle_over = false;
    bool cannot_lose = false;
    bool mugged = false;
    int bomb_id_offset = 0;
};
struct CombatSetup {
    EncounterId encounter = EncounterId::Cultist;
    int floor = 1;
    int hp = 70;
    int max_hp = 70;
    int gold = 99;
    List<Card, card_capacity> deck{};
    List<Relic, 64> relics{};
    PotionId potions[2] = {PotionId::Empty, PotionId::Empty};
};
}
