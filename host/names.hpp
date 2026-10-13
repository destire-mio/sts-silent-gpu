#pragma once
#include "core/state.hpp"
#include <stdexcept>
#include <string>

namespace spire::host {
#define SPIRE_UNIT(id, java_name, header, type) java_name,
constexpr const char* card_names[] = {
#include "core/cards/catalog.def"
};
constexpr const char* relic_names[] = {
#include "core/relics/catalog.def"
};
constexpr const char* potion_names[] = {
#include "core/potions/catalog.def"
};
constexpr const char* power_names[] = {
#include "core/powers/catalog.def"
};
constexpr const char* monster_names[] = {
#include "core/monsters/catalog.def"
};
constexpr const char* encounter_names[] = {
#include "core/encounters/catalog.def"
};
constexpr const char* effect_names[] = {
#include "core/actions/catalog.def"
};
#undef SPIRE_UNIT
constexpr const char* intent_names[] = {"NONE", "ATTACK", "ATTACK_BUFF", "ATTACK_DEBUFF", "ATTACK_DEFEND", "BUFF", "DEBUFF", "STRONG_DEBUFF", "DEFEND", "DEFEND_BUFF", "DEFEND_DEBUFF", "ESCAPE", "MAGIC", "SLEEP", "STUN", "UNKNOWN", "DEBUG"};
constexpr const char* screen_names[] = {"combat", "discard", "victory", "death", "none", "grid", "card_reward"};
constexpr const char* card_type_names[] = {"ATTACK", "SKILL", "POWER", "STATUS", "CURSE"};
constexpr const char* damage_type_names[] = {"NORMAL", "THORNS", "HP_LOSS"};

template<class Id, std::size_t Size>
Id parse_id(const std::string& value, const char* const (&names)[Size]) {
    for (std::size_t i = 0; i < Size; ++i)
        if (value == names[i]) return static_cast<Id>(i);
    throw std::invalid_argument("Unknown ID: " + value);
}

static_assert(sizeof(card_names) / sizeof(*card_names) == static_cast<int>(CardId::Count));
static_assert(sizeof(relic_names) / sizeof(*relic_names) == static_cast<int>(RelicId::Count));
static_assert(sizeof(potion_names) / sizeof(*potion_names) == static_cast<int>(PotionId::Count));
static_assert(sizeof(power_names) / sizeof(*power_names) == static_cast<int>(PowerId::Count));
static_assert(sizeof(monster_names) / sizeof(*monster_names) == static_cast<int>(MonsterId::Count));
static_assert(sizeof(encounter_names) / sizeof(*encounter_names) == static_cast<int>(EncounterId::Count));
static_assert(sizeof(effect_names) / sizeof(*effect_names) == static_cast<int>(EffectKind::Count));
}
