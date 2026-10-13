#pragma once
#include "platform.hpp"

namespace spire {
constexpr int card_capacity = 256;
constexpr int monster_capacity = 8;
constexpr int power_capacity = 64;
constexpr int action_capacity = 128;
constexpr int hand_limit = 10;
constexpr int player_target = -1;
constexpr int no_target = -2;

enum class CardType : uint8_t { Attack, Skill, Power, Status, Curse };
enum class CardTarget : uint8_t { Enemy, Self, None };
enum class Intent : uint8_t { None, Attack, AttackBuff, AttackDebuff, AttackDefend, Buff, Debuff, StrongDebuff, Defend, DefendBuff, Escape, Magic, Sleep, Stun, Unknown, Debug };
enum class DamageType : uint8_t { Normal, Thorns, HpLoss };
enum class AttackEffect : uint8_t { BluntLight, BluntHeavy, SlashDiagonal, Smash, SlashHeavy, SlashHorizontal, SlashVertical, None, Fire, Poison, Shield, Lightning };
enum class MonsterStateChange : uint8_t { Closed, Open, Rear, RearIdle, Attack, UseNet };
enum class Screen : uint8_t { Combat, Discard, Victory, Death, None };
enum class Phase : uint8_t { WaitingOnUser, ExecutingActions };

}
