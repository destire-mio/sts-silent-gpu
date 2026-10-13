#pragma once
#include "fixed.hpp"
#include "random.hpp"
#include "types.hpp"
#include "spire_generated/ids.hpp"

namespace spire {
struct Card {
    CardId id = CardId::Strike_G;
    int16_t upgrades = 0;
    int misc = 0;
    int16_t cost = 0;
    int16_t cost_for_turn = 0;
    int base_damage = -1;
    int damage = -1;
    int base_block = -1;
    int block = -1;
    int base_magic = -1;
    int magic = -1;
    bool exhaust = false;
    bool ethereal = false;
    bool retain = false;
    bool self_retain = false;
    bool innate = false;
    bool free_to_play_once = false;
    bool exhaust_on_use_once = false;
    CardState state{};
};
using Pile = List<uint16_t, card_capacity>;
struct Relic { RelicId id = RelicId::SnakeRing; int counter = -1; RelicState state{}; };
struct Power {
    int amount;
    PowerId id;
    bool just_applied : 1;
    bool triggered : 1;
    bool skip_first : 1;
    PowerState state{};
    SPIRE_FN constexpr Power(PowerId id = PowerId::Strength, int amount = 0)
        : amount(amount), id(id), just_applied(false), triggered(false), skip_first(true) {}
};
using Powers = List<Power, power_capacity>;
struct Creature {
    int hp = 0;
    int max_hp = 0;
    int block = 0;
    int last_damage_taken = 0;
    Powers powers{};
    bool dead = false;
    bool dying = false;
    bool escaped = false;
    bool half_dead = false;
};
struct Monster {
    MonsterId id = MonsterId::Cultist;
    Creature creature{};
    Intent intent = Intent::Debug;
    int8_t move = -1;
    int16_t intent_base_damage = -1;
    int16_t intent_damage = -1;
    int16_t intent_hits = 0;
    int8_t pending_move = -1;
    Intent pending_intent = Intent::None;
    int16_t pending_base_damage = -1;
    int16_t pending_hits = -1;
    RecentHistory<int8_t, 2> move_history{};
    int16_t damage[3]{};
    int16_t block_amount = 0;
    int16_t magic = 0;
    int turn_count = 0;
    int stolen_gold = 0;
    bool first_move = true;
    bool used_special = false;
    bool split_triggered = false;
    bool escape_next = false;
    MonsterState state{};
};
struct Player {
    Creature creature{};
    int energy = 0;
    int energy_master = 3;
    int hand_size = 5;
    int card_in_use = -1;
    int cards_played_this_turn = 0;
    int damaged_this_combat = 0;
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
    EffectKind kind = EffectKind::Draw;
    int8_t source = player_target;
    int8_t target = player_target;
    int amount = 0;
    int16_t card = -1;
    PowerId power = PowerId::Strength;
    int extra = 0;
    DamageType damage_type = DamageType::Normal;
    bool flag = false;
    EffectState state{};
    Power prepared_power{};
};
using DamageInfo = Effect;
struct CardQueueItem { int card = -1; int target = -1; };
struct CardHistory {
    uint16_t values[2]{};
    int size = 0;
    uint32_t types[5]{};
    SPIRE_FN bool empty() const { return size == 0; }
    SPIRE_FN int first_index() const { return max(0, size - 2); }
    SPIRE_FN uint16_t operator[](int index) const {
        SPIRE_ASSERT(index >= first_index() && index < size);
        return values[index % 2];
    }
    SPIRE_FN void push_back(uint16_t card) { values[size++ % 2] = card; }
    SPIRE_FN void record(int card, CardType type) {
        push_back(static_cast<uint16_t>(card));
        ++types[static_cast<int>(type)];
    }
    SPIRE_FN uint32_t count(CardType type) const { return types[static_cast<int>(type)]; }
    SPIRE_FN void clear() {
        size = 0;
        for (int i = 0; i < 5; ++i) types[i] = 0;
    }
};
struct ActionManager {
    Queue<Effect, action_capacity> actions{};
    Queue<Effect, action_capacity> pre_turn_actions{};
    Queue<CardQueueItem, 32> card_queue{};
    Queue<int, monster_capacity> monster_queue{};
    CardHistory cards_played_this_turn{};
    CardHistory cards_played_this_combat{};
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
    List<Relic, 64> relics{};
    PotionId potions[2] = {PotionId::Empty, PotionId::Empty};
    PotionState potion_state[2]{};
};
struct Game {
    Run run{};
    RngStreams rng{};
    JavaRandom unseeded{0};
    Player player{};
    List<Card, card_capacity> cards{};
    Pile hand{}, draw{}, discard{}, exhaust{}, retained{};
    Pile drawn_cards{};
    List<Monster, monster_capacity> monsters{};
    List<int, monster_capacity> monster_order{};
    ActionManager manager{};
    Screen screen = Screen::None;
    EncounterId encounter = EncounterId::Cultist;
    EncounterState encounter_state{};
    int turn = 1;
    int discard_remaining = 0;
    bool battle_over = false;
    bool mugged = false;
};
struct CombatSetup {
    EncounterId encounter = EncounterId::Cultist;
    int floor = 1;
    int hp = 70;
    int max_hp = 70;
    int gold = 99;
    uint64_t unseeded_seed = 0;
    List<Card, card_capacity> deck{};
    List<Relic, 64> relics{};
    PotionId potions[2] = {PotionId::Empty, PotionId::Empty};
};
}
