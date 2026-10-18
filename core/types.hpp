#pragma once
#include "core/platform.hpp"

namespace spire {
constexpr int card_capacity = 221;
constexpr int master_deck_capacity = 128;
constexpr int card_pool_capacity = 64;
constexpr int monster_capacity = 64;
constexpr int power_capacity = 32;
constexpr int power_object_capacity = 128;
constexpr int action_capacity = 64;
constexpr int hand_limit = 10;
constexpr int potion_capacity = 8;
constexpr int room_reward_capacity = 16;
constexpr int reward_card_capacity = 8;
static_assert(card_capacity <= 256 && power_object_capacity <= 256 && action_capacity <= 128);
static_assert(monster_capacity < 128, "Card multi-damage count uses seven bits");
constexpr int player_target = -1;
constexpr int no_target = -2;

enum class OrbKind : uint8_t { Lightning, Frost, Dark, Plasma };
enum class Stance : uint8_t { Neutral, Wrath, Calm, Divinity };
enum class CardType : uint8_t { Attack, Skill, Power, Status, Curse };
enum class CardRarity : uint8_t { Basic, Common, Uncommon, Rare, Special, Curse };
enum class CardTarget : uint8_t { Enemy, Self, None, AllEnemies, All, SelfAndEnemy };
enum class Intent : uint8_t { None, Attack, AttackBuff, AttackDebuff, AttackDefend, Buff, Debuff, StrongDebuff, Defend, DefendBuff, DefendDebuff, Escape, Magic, Sleep, Stun, Unknown, Debug };
enum class DamageType : uint8_t { Normal, Thorns, HpLoss };
enum class EnemyType : uint8_t { Normal, Elite, Boss };
enum class Screen : uint8_t { Combat, Discard, Grid, Victory, Death, None, CardReward, CombatReward, Event, Map, Room, Shop, BossReward };
enum class PileId : uint8_t { Hand, Draw, Discard, Exhaust, Limbo, Grid, MasterDeck };
enum class EventChoiceKind : uint8_t { Option, Card, Control };
struct EventChoice { EventChoiceKind kind; uint8_t index; };
enum class RoomPhase : uint8_t { Incomplete, Combat, Complete, Event };
enum class CampfireOption : uint8_t { Rest, Smith, Lift, Toke, Dig, Recall, Count };
enum class Phase : uint8_t { WaitingOnUser, ExecutingActions };
enum class RoomKind : uint8_t { Monster, Elite, Boss, Shop, Rest, Treasure, Event, Empty, Neow, BossTreasure, Victory, HeartEvent };
enum class RelicTier : uint8_t { Common, Uncommon, Rare, Shop, Boss };

}
