#pragma once
#include "fixed.hpp"
#include "random.hpp"
#include "card_catalog.hpp"

namespace spire {
constexpr int card_capacity = 512;
constexpr int monster_capacity = 8;
constexpr int power_capacity = 64;
constexpr int action_capacity = 1024;
constexpr int hand_limit = 10;
constexpr int player_target = -1;
constexpr int no_target = -2;

#define SPIRE_CARD_ID(id, ...) id,
enum class CardId : uint16_t { SPIRE_CARD_CATALOG(SPIRE_CARD_ID) Count };
#undef SPIRE_CARD_ID
enum class CardType : uint8_t { Attack, Skill, Power, Status, Curse };
enum class CardTarget : uint8_t { Enemy, Self, None };
enum class RelicId : uint16_t { SnakeRing, Count };
enum class PotionId : uint16_t { Empty, Count };
enum class PowerId : uint16_t {
    Strength, Dexterity, Weak, Vulnerable, Frail, Ritual, CurlUp, Angry,
    SporeCloud, Entangled, Split, Thievery, Artifact, NoDraw, Count
};
enum class MonsterId : uint16_t {
    Cultist, JawWorm, LouseNormal, LouseDefensive, AcidSlime_S, AcidSlime_M,
    AcidSlime_L, SpikeSlime_S, SpikeSlime_M, SpikeSlime_L, FungiBeast,
    SlaverBlue, SlaverRed, Looter, GremlinWarrior, GremlinThief, GremlinFat,
    GremlinTsundere, GremlinWizard, Count
};
enum class EncounterId : uint16_t {
    Cultist, JawWorm, TwoLouse, SmallSlimes, BlueSlaver, GremlinGang, Looter,
    LargeSlime, LotsOfSlimes, ExordiumThugs, ExordiumWildlife, RedSlaver,
    ThreeLouse, TwoFungiBeasts, Count
};
enum class Intent : uint8_t { None, Attack, AttackBuff, AttackDebuff, AttackDefend, Buff, Debuff, StrongDebuff, Defend, DefendBuff, Escape, Magic, Sleep, Stun, Unknown, Debug };
enum class DamageType : uint8_t { Normal, Thorns, HpLoss };
enum class Screen : uint8_t { Combat, Discard, Victory, Death, None };
enum class Phase : uint8_t { WaitingOnUser, ExecutingActions };

struct Card {
    CardId id = CardId::Strike_G;
    int upgrades = 0;
    int misc = 0;
    int cost = 0;
    int cost_for_turn = 0;
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
};
using Pile = List<int, card_capacity>;
struct Relic { RelicId id = RelicId::SnakeRing; int counter = -1; };
struct Power {
    PowerId id = PowerId::Strength;
    int amount = 0;
    bool just_applied = false;
    bool triggered = false;
    bool skip_first = true;
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
    int move = -1;
    int intent_base_damage = -1;
    int intent_damage = -1;
    int intent_hits = 0;
    int pending_move = -1;
    Intent pending_intent = Intent::None;
    int pending_base_damage = -1;
    int pending_hits = -1;
    List<int, 1024> move_history{};
    int damage[3]{};
    int block_amount = 0;
    int magic = 0;
    int turn_count = 0;
    int stolen_gold = 0;
    bool first_move = true;
    bool used_special = false;
    bool split_triggered = false;
};
struct Player {
    Creature creature{};
    int energy = 0;
    int energy_master = 3;
    int hand_size = 5;
    int card_in_use = -1;
    int cards_played_this_turn = 0;
    int damaged_this_combat = 0;
};
enum class ActionKind : uint8_t { Play, EndTurn, Choose, PotionUse, PotionDiscard, Proceed, Skip };
struct Action {
    ActionKind kind = ActionKind::EndTurn;
    int index = -1;
    int target = -1;
};
using ActionSet = List<Action, 128>;
enum class EffectKind : uint8_t {
    Damage, Block, ApplyPower, ReducePower, RemovePower, Draw, Shuffle,
    Discard, UseCard, EndTurnDiscard, RestoreRetained, EndTurn, MonsterStart,
    RollMove, SetMove, Escape, AddCardDiscard, Split,
    StealGold, BlockRandomMonster, ExhaustCard
};
struct Effect {
    EffectKind kind = EffectKind::Draw;
    int source = player_target;
    int target = player_target;
    int amount = 0;
    int card = -1;
    PowerId power = PowerId::Strength;
    DamageType damage_type = DamageType::Normal;
    int extra = 0;
    bool flag = false;
};
struct CardQueueItem { int card = -1; int target = -1; };
struct ActionManager {
    Queue<Effect, action_capacity> actions{};
    Queue<Effect, action_capacity> pre_turn_actions{};
    Queue<CardQueueItem, 32> card_queue{};
    Queue<int, monster_capacity> monster_queue{};
    List<int, 2048> cards_played_this_turn{};
    List<int, 8192> cards_played_this_combat{};
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
