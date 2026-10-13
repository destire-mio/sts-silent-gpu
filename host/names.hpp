#pragma once
#include "core/state.hpp"
#include "core/card_catalog.hpp"
#include <stdexcept>
#include <string>

namespace spire::host {
#define SPIRE_CARD_NAME(id, java_name, ...) java_name,
constexpr const char* card_names[] = {SPIRE_CARD_CATALOG(SPIRE_CARD_NAME)};
#undef SPIRE_CARD_NAME
constexpr const char* relic_names[] = {"Ring of the Snake"};
constexpr const char* potion_names[] = {"Empty"};
constexpr const char* power_names[] = {"Strength", "Dexterity", "Weakened", "Vulnerable", "Frail", "Ritual", "Curl Up", "Angry", "Spore Cloud", "Entangled", "Split", "Thievery", "Artifact", "No Draw"};
constexpr const char* monster_names[] = {"Cultist", "JawWorm", "FuzzyLouseNormal", "FuzzyLouseDefensive", "AcidSlime_S", "AcidSlime_M", "AcidSlime_L", "SpikeSlime_S", "SpikeSlime_M", "SpikeSlime_L", "FungiBeast", "SlaverBlue", "SlaverRed", "Looter", "GremlinWarrior", "GremlinThief", "GremlinFat", "GremlinTsundere", "GremlinWizard"};
constexpr const char* encounter_names[] = {"Cultist", "Jaw Worm", "2 Louse", "Small Slimes", "Blue Slaver", "Gremlin Gang", "Looter", "Large Slime", "Lots of Slimes", "Exordium Thugs", "Exordium Wildlife", "Red Slaver", "3 Louse", "2 Fungi Beasts"};
constexpr const char* intent_names[] = {"NONE", "ATTACK", "ATTACK_BUFF", "ATTACK_DEBUFF", "ATTACK_DEFEND", "BUFF", "DEBUFF", "STRONG_DEBUFF", "DEFEND", "DEFEND_BUFF", "ESCAPE", "MAGIC", "SLEEP", "STUN", "UNKNOWN", "DEBUG"};
constexpr const char* screen_names[] = {"combat", "discard", "victory", "death", "none"};
constexpr const char* effect_names[] = {"damage", "block", "apply_power", "reduce_power", "remove_power", "draw", "shuffle", "discard", "use_card", "end_turn_discard", "restore_retained", "end_turn", "monster_start", "roll_move", "set_move", "escape", "add_card_discard", "split", "steal_gold", "block_random_monster", "exhaust_card"};
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
}
