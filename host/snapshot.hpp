#pragma once
#include "names.hpp"
#include "run_state.hpp"
#include "core/card_pool.hpp"
#include "core/card_data.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <array>
#include <charconv>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include "spire_host_generated/snapshot.hpp"

namespace spire::host {
namespace py = pybind11;
using namespace pybind11::literals;

inline py::dict snapshot_inventory() {
    return py::module_::import("json").attr("loads")(snapshot_metadata).cast<py::dict>();
}

inline py::float_ java_float(float value) {
    char text[48];
    const auto end = std::to_chars(text, text + sizeof(text), value).ptr;
    return py::float_(std::stod(std::string(text, end)));
}

struct UnitStateSnapshot {
    py::dict fields;
    template<class T, size_t N> void operator()(const char* name, const T (&value)[N]) {
        if constexpr (std::is_same_v<T, char>) fields[py::str(name)] = py::str(value);
        else {
            py::list items;
            for (const T& item : value) items.append(py::cast(item));
            fields[py::str(name)] = items;
        }
    }
    template<class T> void operator()(const char* name, const T& value) {
        std::string field;
        bool upper = false;
        const bool java_constant = *name >= 'A' && *name <= 'Z';
        for (const char* next = name; *next; ++next) {
            if (java_constant) { field += *next; continue; }
            if (*next == '_') { upper = true; continue; }
            field += upper ? static_cast<char>(*next - 'a' + 'A') : *next;
            upper = false;
        }
        if constexpr (std::is_array_v<T>) {
            py::list items;
            for (const auto& item : value) items.append(py::cast(item));
            fields[py::str(field)] = items;
        } else if constexpr (std::is_enum_v<T>) fields[py::str(field)] = py::cast(static_cast<std::underlying_type_t<T>>(value));
        else fields[py::str(field)] = py::cast(value);
    }
};

struct SnapshotGraph {
    const Game& game;
    py::dict inventory = snapshot_inventory();
    py::list objects{};
    std::map<std::array<int, 3>, int> identities;
    std::map<uint32_t, int> uuids;
    std::set<std::string> gaps;

    py::dict metadata(const char* kind, int id) const {
        return inventory["units"].cast<py::dict>()[kind].cast<py::list>()[id].cast<py::dict>();
    }

    std::string class_name(const char* kind, int id) const {
        return metadata(kind, id)["class"].cast<std::string>();
    }

    std::map<std::string, std::string> fields_for(std::string name) const {
        std::map<std::string, std::string> fields;
        const py::dict schema = inventory["schema"].cast<py::dict>();
        while (!name.empty()) {
            const py::dict definition = schema[py::str(name)].cast<py::dict>();
            const std::string simple = name.substr(name.rfind('.') + 1);
            for (auto field : definition["fields"].cast<py::dict>())
                fields[simple + "." + py::cast<std::string>(field.first)] = py::cast<std::string>(field.second);
            name = definition["extends"].is_none() ? "" : definition["extends"].cast<std::string>();
        }
        return fields;
    }

    template<class Build>
    py::dict object(std::array<int, 3> identity, const std::string& name, Build build) {
        auto found = identities.find(identity);
        if (found != identities.end()) return py::dict("ref"_a=found->second);
        const int index = static_cast<int>(objects.size());
        identities.emplace(identity, index);
        py::dict fields;
        objects.append(py::dict("class"_a=name, "fields"_a=fields));
        for (const auto& field : fields_for(name)) {
            const py::object value = build(field.first);
            if (value) fields[py::str(field.first)] = value;
            else gaps.insert(name + "." + field.first);
        }
        return py::dict("ref"_a=index);
    }

    py::object card_value(const Card& card, bool master, int index) {
        const int id = static_cast<int>(card.id);
        const py::dict data = metadata("cards", id);
        const auto definition = card_data(card.id);
        return object({3, master ? 1 : 0, index}, class_name("cards", id), [&](const std::string& key) -> py::object {
            const std::string field = key.substr(key.find('.') + 1);
            if (field == "baseBlock") return py::int_(card.base_block);
            if (field == "baseDamage") return py::int_(card.base_damage);
            if (field == "baseMagicNumber") return py::int_(card.base_magic);
            if (field == "block") return py::int_(card.block);
            if (field == "damage") return py::int_(card.damage);
            if (field == "magicNumber") return py::int_(card.magic);
            if (field == "cardID") return py::str(card_names[id]);
            if (field == "color" || field == "rarity" || field == "type" || field == "target" || field == "tags")
                return py::reinterpret_borrow<py::object>(data[py::str(field)]);
            if (field == "cost") return py::int_(card.cost);
            if (field == "costForTurn") return py::int_(card.cost_for_turn);
            if (field == "misc") return py::int_(card.misc);
            if (field == "energyOnUse") return py::int_(card.energy_on_use);
            if (field == "dontTriggerOnUseCard") return py::bool_(card.dont_trigger_on_use_card);
            if (field == "isInAutoplay") return py::bool_(card.in_autoplay);
            if (field == "ignoreEnergyOnUse") return py::bool_(card.ignore_energy_on_use);
            if (field == "timesUpgraded") return py::int_(card.upgrades);
            if (field == "upgraded") return py::bool_(card.upgrades > 0);
            if (field == "upgradedDamage") return py::bool_(card.upgrades > 0 && definition.upgrade_damage != 0);
            if (field == "upgradedBlock") return py::bool_(card.upgrades > 0 && definition.upgrade_block != 0);
            if (field == "upgradedMagicNumber") return py::bool_(card.upgrades > 0 && definition.upgrade_magic != 0);
            if (field == "exhaust") return py::bool_(card.exhaust);
            if (field == "isEthereal") return py::bool_(card.ethereal);
            if (field == "isInnate") return py::bool_(card.innate);
            if (field == "retain") return py::bool_(card.retain);
            if (field == "selfRetain") return py::bool_(card.self_retain);
            if (field == "freeToPlayOnce") return py::bool_(card.free_to_play_once);
            if (field == "exhaustOnUseOnce") return py::bool_(card.exhaust_on_use_once);
            if (field == "purgeOnUse") return py::bool_(card.purge_on_use);
            if (field == "returnToHand") return py::bool_(card.return_to_hand);
            if (field == "shuffleBackIntoDrawPile") return py::bool_(card.shuffle_back_into_draw_pile);
            if (field == "uuid") {
                const auto inserted = uuids.emplace(card.uuid, static_cast<int>(uuids.size()));
                return py::int_(inserted.first->second);
            }
            if (field == "damageType" || field == "damageTypeForTurn") return py::str("NORMAL");
            if (field == "baseDiscard" || field == "baseDraw" || field == "baseHeal" ||
                field == "discard" || field == "draw" || field == "heal") return py::int_(-1);
            if (field == "multiDamage") {
                if (master) return py::none();
                const auto& combat_card = game.cards[index];
                if (combat_card.multi_damage_count == 0) return py::none();
                py::list result;
                for (int i = 0; i < combat_card.multi_damage_count; ++i) result.append(game.multi_damage[index][i]);
                return result;
            }
            if (field == "price") return py::int_(0);
            if (field == "exhaustOnFire" || field == "inBottleFlame" ||
                field == "inBottleLightning" || field == "inBottleTornado" || field == "isFlipped" ||
                field == "isMagicNumberModified" || field == "isMultiDamage" ||
                field == "isSelected" || field == "isUsed" || field == "upgradedCost") return py::bool_(false);
            return {};
        });
    }

    py::object card(int index) {
        return index < 0 ? py::object(py::none()) : card_value(game.cards[index], false, index);
    }

    template<class Cards>
    py::list cards(const Cards& values) {
        py::list result;
        for (int i = 0; i < values.size; ++i) result.append(card(values[i]));
        return result;
    }

    py::object group(int id, const char* type, const Pile* pile = nullptr) {
        return object({2, id, 0}, "cards.CardGroup", [&](const std::string& key) -> py::object {
            if (key == "CardGroup.group") {
                if (pile) return cards(*pile);
                py::list result;
                if (id == 5) for (int i = 0; i < game.run.master_deck.size; ++i)
                    result.append(card_value(game.run.master_deck[i], true, i));
                return result;
            }
            if (key == "CardGroup.queued") return py::list();
            if (key == "CardGroup.type") return py::str(type);
            return {};
        });
    }

    py::object creature(int owner) { return owner == no_target ? py::object(py::none()) : owner == player_target ? player() : monster(owner); }

    py::object power_value(const Power& power, int owner, std::array<int, 3> identity) {
        const PowerData data = power_data(power.id);
        UnitStateSnapshot state;
        power_visit_state(power, state);
        if (power.instance != 0) identity = {6, static_cast<int>(power.instance), 0};
        return object(identity, class_name("powers", static_cast<int>(power.id)), [&](const std::string& key) -> py::object {
            const std::string field = key.substr(key.find('.') + 1);
            if (field == "ID") {
                std::string id = power_names[static_cast<int>(power.id)];
                if (power.id_offset >= 0) id += std::to_string(power.id_offset);
                return py::str(id);
            }
            if (field == "owner") return creature(owner);
            if (field == "amount") return py::int_(power.amount);
            if (field == "priority") return py::int_(data.priority);
            if (field == "type") return py::str(power_isDebuff(power, owner) ? "DEBUFF" : "BUFF");
            if (field == "canGoNegative") return py::bool_(data.can_negative);
            if (field == "isTurnBased") return py::bool_(data.turn_based);
            if (field == "isPostActionPower") return py::bool_(data.post_action);
            const py::object value = powers_snapshot(*this, power, owner, key);
            if (value) return value;
            if (state.fields.contains(py::str(field))) return py::reinterpret_borrow<py::object>(state.fields[py::str(field)]);
            return {};
        });
    }

    py::object power(int owner, int index) {
        return power_value(get_creature(game, owner).powers.at(game.power_objects, index), owner, {6, owner, index});
    }

    py::object creature_field(int owner, const std::string& key) {
        const Creature& c = get_creature(game, owner);
        if (key == "AbstractCreature.currentBlock") return py::int_(c.block);
        if (key == "AbstractCreature.currentHealth") return py::int_(c.hp);
        if (key == "AbstractCreature.drawX") {
            if (owner != player_target && c.escaped) return py::none();
            return java_float(c.draw_x);
        }
        if (key == "AbstractCreature.flipHorizontal") return py::bool_(c.flip_horizontal);
        if (key == "AbstractCreature.gold") return py::int_(owner == player_target ? game.run.gold : c.gold);
        if (key == "AbstractCreature.halfDead") return py::bool_(c.half_dead);
        if (key == "AbstractCreature.id") return owner == player_target ? py::object(py::none()) : py::object(py::str(monster_names[static_cast<int>(game.monsters[owner].id)]));
        if (key == "AbstractCreature.isBloodied") return py::bool_(owner == player_target && game.player.bloodied);
        if (key == "AbstractCreature.isDead") return py::bool_(c.dead);
        if (key == "AbstractCreature.isDying") return py::bool_(c.dying);
        if (key == "AbstractCreature.isEscaping") return py::bool_(owner == player_target ? game.player.escaping : c.escaped);
        if (key == "AbstractCreature.isPlayer") return py::bool_(owner == player_target);
        if (key == "AbstractCreature.lastDamageTaken") return py::int_(c.last_damage_taken);
        if (key == "AbstractCreature.maxHealth") return py::int_(c.max_hp);
        if (key == "AbstractCreature.powers") {
            py::list result;
            for (int i = 0; i < c.powers.size; ++i) result.append(power(owner, i));
            return result;
        }
        return {};
    }

    py::object relic(int index) {
        const Relic& relic = game.run.relics[index];
        return object({9, index, 0}, class_name("relics", static_cast<int>(relic.id)), [&](const std::string& key) -> py::object {
            if (key == "AbstractRelic.relicId") return py::str(relic_names[static_cast<int>(relic.id)]);
            if (key == "AbstractRelic.counter") return py::int_(relic.counter);
            if (key == "AbstractRelic.cost") return py::int_(0);
            if (key == "AbstractRelic.tier") return py::reinterpret_borrow<py::object>(metadata("relics", static_cast<int>(relic.id))["tier"]);
            if (key == "AbstractRelic.isObtained") return py::bool_(true);
            if (key == "AbstractRelic.energyBased" || key == "AbstractRelic.usedUp" || key == "AbstractRelic.grayscale" || key == "AbstractRelic.discarded") return py::bool_(false);
            return {};
        });
    }

    py::object potion(int index) {
        const PotionId id = game.run.potions[index];
        const bool empty = id == PotionId::Empty;
        return object({10, index, 0}, class_name("potions", static_cast<int>(id)), [&](const std::string& key) -> py::object {
            if (key == "AbstractPotion.ID") return py::str(potion_names[static_cast<int>(id)]);
            if (key == "AbstractPotion.potency") return py::int_(potion_potency(game, index));
            if (key == "AbstractPotion.slot") return py::int_(empty ? -1 : index);
            if (key == "AbstractPotion.rarity" && empty) return py::str("PLACEHOLDER");
            if (key == "AbstractPotion.canUse") return py::bool_(false);
            if (key == "AbstractPotion.targetRequired") return py::bool_(potion_data(id).target == CardTarget::Enemy);
            if (!empty) {
                const py::dict fields = inventory["rewards"].cast<py::dict>()["potions"].cast<py::dict>()[py::str(potion_names[static_cast<int>(id)])].cast<py::dict>()["fields"].cast<py::dict>();
                if (fields.contains(py::str(key))) return py::reinterpret_borrow<py::object>(fields[py::str(key)]);
            }
            return {};
        });
    }

    py::object player() {
        return object({1, 0, 0}, "characters.TheSilent", [&](const std::string& key) -> py::object {
            if (key.compare(0, 17, "AbstractCreature.") == 0) return creature_field(player_target, key);
            const std::string field = key.substr(key.find('.') + 1);
            if (field == "blights" || field == "orbs") return py::list();
            if (field == "cardInUse") return card(game.player.card_in_use);
            if (field == "cardsPlayedThisTurn") return py::int_(game.player.cards_played_this_turn);
            if (field == "chosenClass") return py::str("THE_SILENT");
            if (field == "discardPile") return group(0, "DISCARD_PILE", &game.discard);
            if (field == "drawPile") return group(1, "DRAW_PILE", &game.draw);
            if (field == "endTurnQueued" || field == "isEndingTurn") return py::bool_(false);
            if (field == "energy") return object({4, 0, 0}, "core.EnergyManager", [&](const std::string& name) -> py::object {
                if (name == "EnergyManager.energy") return py::int_(game.player.energy_per_turn);
                if (name == "EnergyManager.energyMaster") return py::int_(game.player.energy_master);
                return {};
            });
            if (field == "exhaustPile") return group(2, "EXHAUST_PILE", &game.exhaust);
            if (field == "gameHandSize") return py::int_(game.player.hand_size);
            if (field == "masterHandSize") return py::int_(game.player.master_hand_size);
            if (field == "hand") return group(3, "HAND", &game.hand);
            if (field == "hoveredCard") return py::none();
            if (field == "limbo") return group(4, "UNSPECIFIED", &game.limbo);
            if (field == "masterDeck") return group(5, "MASTER_DECK");
            if (field == "masterMaxOrbs" || field == "maxOrbs") return py::int_(0);
            if (field == "potionSlots") return py::int_(2);
            if (field == "potions") {
                py::list result;
                for (int i = 0; i < 2; ++i) result.append(potion(i));
                return result;
            }
            if (field == "relics") {
                py::list result;
                for (int i = 0; i < game.run.relics.size; ++i) result.append(relic(i));
                return result;
            }
            if (field == "stance") return object({11, 0, 0}, "stances.NeutralStance", [](const std::string& name) -> py::object {
                return name == "AbstractStance.ID" ? py::object(py::str("Neutral")) : py::object();
            });
            if (field == "startingMaxHP") return py::int_(70);
            return {};
        });
    }

    py::object monster(int index) {
        const Monster& monster = game.monsters[index];
        UnitStateSnapshot state;
        monster_visit_state(monster, state);
        return object({5, index, 0}, class_name("monsters", static_cast<int>(monster.id)), [&](const std::string& key) -> py::object {
            if (key.compare(0, 17, "AbstractCreature.") == 0) return creature_field(index, key);
            const std::string field = key.substr(key.find('.') + 1);
            if (state.fields.contains(py::str(field))) return py::reinterpret_borrow<py::object>(state.fields[py::str(field)]);
            if (field == "cannotEscape") return py::bool_(false);
            if (field == "damage") {
                py::list result;
                for (int i = 0; i < monster.damage_count; ++i) result.append(monster_damage(index, i));
                return result;
            }
            if (field == "escaped") return py::bool_(monster.creature.escaped);
            if (field == "escapeNext") return py::bool_(monster.escape_next);
            if (field == "intent") return py::str(intent_names[static_cast<int>(monster.intent)]);
            if (field == "intentBaseDmg") return py::int_(monster.intent_base_damage);
            if (field == "intentDmg") return py::int_(monster.intent_damage);
            if (field == "intentMultiAmt") return py::int_(monster.intent_hits);
            if (field == "isMultiDmg") return py::bool_(monster.intent_multi);
            if (field == "moveHistory") {
                py::list result;
                for (int i = 0; i < monster.move_history.size; ++i) result.append(monster.move_history[i]);
                return result;
            }
            if (field == "nextMove") return py::int_(monster.move);
            if (field == "type") {
                const char* types[] = {"NORMAL", "ELITE", "BOSS"};
                return py::str(types[static_cast<int>(monster_data(monster.id).type)]);
            }
            if (field == "move") return object({8, index, 0}, "monsters.EnemyMoveInfo", [&](const std::string& name) -> py::object {
                if (name == "EnemyMoveInfo.baseDamage") return py::int_(monster.pending_base_damage);
                if (name == "EnemyMoveInfo.intent") return py::str(intent_names[static_cast<int>(monster.pending_intent)]);
                if (name == "EnemyMoveInfo.isMultiDamage") return py::bool_(monster.pending_multi);
                if (name == "EnemyMoveInfo.multiplier") return py::int_(monster.pending_hits);
                if (name == "EnemyMoveInfo.nextMove") return py::int_(monster.pending_move);
                return {};
            });
            if (field == "splitTriggered") return py::bool_(monster.split_triggered);
            if (field == "saveX") return java_float(monster.offset_x);
            if (key == "Cultist.ritualAmount") return py::int_(4);
            if (key == "Cultist.talky") return py::bool_(true);
            return {};
        });
    }

    py::object reward_card(const FreshRunState& fresh, int index) {
        const py::dict data = inventory["rewards"].cast<py::dict>()["cards"].cast<py::dict>()[py::str(fresh.rewardCards[index])].cast<py::dict>();
        const py::dict fields = data["fields"].cast<py::dict>();
        return object({21, index, 0}, data["class"].cast<std::string>(), [&](const std::string& key) -> py::object {
            if (key == "AbstractCard.uuid") {
                const auto inserted = uuids.emplace(0x80000000u + index, static_cast<int>(uuids.size()));
                return py::int_(inserted.first->second);
            }
            return py::reinterpret_borrow<py::object>(fields[py::str(key)]);
        });
    }

    py::object reward_potion(const FreshRunState& fresh) {
        const py::dict data = inventory["rewards"].cast<py::dict>()["potions"].cast<py::dict>()[py::str(fresh.rewardPotion)].cast<py::dict>();
        const py::dict fields = data["fields"].cast<py::dict>();
        return object({22, 0, 0}, data["class"].cast<std::string>(), [&](const std::string& key) -> py::object {
            if (key == "AbstractPotion.potency" && fields[py::str(key)].is_none())
                return py::int_(std::size(game.run.potions));
            return py::reinterpret_borrow<py::object>(fields[py::str(key)]);
        });
    }

    py::object reward(const FreshRunState& fresh, int index) {
        const std::string& type = fresh.rewardTypes[index];
        return object({23, index, 0}, "rewards.RewardItem", [&](const std::string& key) -> py::object {
            if (key == "RewardItem.type") return py::str(type);
            if (key == "RewardItem.goldAmt") return py::int_(type == "GOLD" ? fresh.rewardGold : type == "STOLEN_GOLD" ? fresh.stolenGold : 0);
            if (key == "RewardItem.bonusGold") return py::int_(0);
            if (key == "RewardItem.ignoreReward" || key == "RewardItem.isDone") return py::bool_(false);
            if (key == "RewardItem.relic" || key == "RewardItem.relicLink") return py::none();
            if (key == "RewardItem.potion") return type == "POTION" ? reward_potion(fresh) : py::object(py::none());
            if (key == "RewardItem.cards") {
                if (type != "CARD") return py::none();
                py::list result;
                for (int i = 0; i < static_cast<int>(fresh.rewardCards.size()); ++i) result.append(reward_card(fresh, i));
                return result;
            }
            return {};
        });
    }

    py::object reward_screen(const FreshRunState& fresh) {
        return object({24, 0, 0}, "screens.CombatRewardScreen", [&](const std::string& key) -> py::object {
            if (key == "CombatRewardScreen.mug") return py::bool_(game.mugged);
            if (key == "CombatRewardScreen.smoke") return py::bool_(game.smoked);
            if (key == "CombatRewardScreen.hasTakenAll") return py::bool_(!game.smoked && fresh.rewardTypes.empty());
            if (key == "CombatRewardScreen.rewards") {
                py::list result;
                if (!game.smoked)
                    for (int i = 0; i < static_cast<int>(fresh.rewardTypes.size()); ++i) result.append(reward(fresh, i));
                return result;
            }
            return {};
        });
    }

    py::object room(const FreshRunState& fresh) {
        return object({12, 0, 0}, "rooms.MonsterRoom", [&](const std::string& key) -> py::object {
            const std::string field = key.substr(key.find('.') + 1);
            if (field == "baseRareCardChance" || field == "rareCardChance") return py::int_(3);
            if (field == "baseUncommonCardChance" || field == "uncommonCardChance") return py::int_(37);
            if (field == "cannotLose") return py::bool_(game.cannot_lose);
            if (field == "combatEvent" || field == "eliteTrigger" || field == "skipMonsterTurn") return py::bool_(false);
            if (field == "smoked") return py::bool_(game.smoked);
            if (field == "event") return py::none();
            if (field == "endBattleTimer" && !game.battle_over) return py::float_(0.0);
            if (field == "isBattleOver") return py::bool_(game.battle_over);
            if (field == "monsters") return object({13, 0, 0}, "monsters.MonsterGroup", [&](const std::string& name) -> py::object {
                if (name != "MonsterGroup.monsters") return {};
                py::list result;
                for (int i = 0; i < game.monster_order.size; ++i) result.append(monster(game.monster_order[i]));
                return result;
            });
            if (field == "mugged") return py::bool_(game.mugged);
            if (field == "phase") return py::str(game.battle_over ? "COMPLETE" : "COMBAT");
            if (field == "potions" || field == "relics") return py::list();
            if (field == "rewardAllowed") return py::bool_(true);
            if (field == "rewardTime") return py::bool_(game.battle_over);
            if (field == "rewards") {
                py::list result;
                for (int i = 0; i < static_cast<int>(fresh.rewardTypes.size()); ++i)
                    if (fresh.rewardTypes[i] != "CARD") result.append(reward(fresh, i));
                return result;
            }
            return {};
        });
    }

    py::object monster_damage(int owner, int index) {
        const Monster& monster = game.monsters[owner];
        return object({7, owner, index}, "cards.DamageInfo", [&](const std::string& field) -> py::object {
            if (field == "DamageInfo.base") return py::int_(monster.damage[index]);
            if (field == "DamageInfo.output") return py::int_(monster.damage_output[index]);
            if (field == "DamageInfo.isModified") return py::bool_(monster.damage_modified[index]);
            if (field == "DamageInfo.owner") return creature(owner);
            if (field == "DamageInfo.type") return py::str("NORMAL");
            return {};
        });
    }

    template<class T> py::object scalar(const T& value) { return py::cast(value); }
    py::object scalar(const char* value) { return value ? py::object(py::str(value)) : py::object(py::none()); }
    py::object scalar(std::nullptr_t) { return py::none(); }
    py::object uuid(uint32_t value) { return py::int_(uuids.emplace(value, static_cast<int>(uuids.size())).first->second); }
    py::object damage_type(DamageType type) { return py::str(damage_type_names[static_cast<int>(type)]); }
    py::object card_type(int type) { return type < 0 ? py::object(py::none()) : py::object(py::str(card_type_names[type])); }
    py::object intent(int value) { return py::str(intent_names[value]); }
    template<class Index> py::object card_array(const Index* values, int count) {
        py::list result;
        for (int i = 0; i < count; ++i) result.append(card(values[i]));
        return result;
    }
    py::object damage_array(const int* values, int count, int card, bool initialized) {
        if (!initialized) return py::none();
        py::list result;
        for (int i = 0; i < count; ++i) result.append(card >= 0 ? game.multi_damage[card][i] : values[i]);
        return result;
    }
    py::object optional_pile_group(PileId pile, bool present) { return present ? pile_group(pile) : py::object(py::none()); }
    py::object power_id(PowerId id, uint32_t instance) {
        return instance == 0 ? py::object(py::str(power_names[static_cast<int>(id)])) : py::object(py::none());
    }
    py::object power_reference(int owner, uint32_t instance) {
        if (instance == 0) return py::none();
        return power_value(power_instance(game, instance), owner, {6, static_cast<int>(instance), 0});
    }
    py::object action_reference(int index) {
        return index < 0 ? py::object(py::none()) : effect(game.action_objects[index], 3, index);
    }
    py::object potion_template(PotionId potion, int queue, int index) {
        const py::dict data = inventory["rewards"].cast<py::dict>()["potions"].cast<py::dict>()[py::str(potion_names[static_cast<int>(potion)])].cast<py::dict>();
        return object({25, queue, index}, data["class"].cast<std::string>(), [&](const std::string& key) -> py::object {
            return py::reinterpret_borrow<py::object>(data["fields"].cast<py::dict>()[py::str(key)]);
        });
    }
    py::object damage_info(const Effect& effect, int queue, int index) {
        if (effect.source >= 0 && effect.damage_type == DamageType::Normal) {
            const Monster& owner = game.monsters[effect.source];
            for (int i = 0; i < owner.damage_count; ++i)
                if (owner.damage[i] == effect.info_base) return monster_damage(effect.source, i);
        }
        return object({16, queue, index}, "cards.DamageInfo", [&](const std::string& name) -> py::object {
            if (name == "DamageInfo.base") return py::int_(effect.info_base);
            if (name == "DamageInfo.output") return py::int_(effect.amount);
            if (name == "DamageInfo.isModified") return py::bool_(false);
            if (name == "DamageInfo.owner") return creature(effect.source);
            if (name == "DamageInfo.type") return damage_type(effect.damage_type);
            return {};
        });
    }
    py::object effect(const Effect& effect, int queue, int index) {
        const std::string name = effect_names[static_cast<int>(effect.kind)];
        if (!inventory["schema"].cast<py::dict>().contains(py::str(name))) return {};
        UnitStateSnapshot state;
        action_visit_state(effect, state);
        return object({15, queue, index}, name, [&](const std::string& key) -> py::object {
            const py::object value = actions_snapshot(*this, effect, queue, index, key);
            if (value) return value;
            const std::string field = key.substr(key.find('.') + 1);
            if (key == "AbstractGameAction.actionType") return scalar(action_data(effect.kind).type);
            if (key == "AbstractGameAction.amount") return py::int_(effect.amount);
            if (key == "AbstractGameAction.damageType") return py::none();
            if (key == "AbstractGameAction.isDone") return py::bool_(effect.done);
            if (key == "AbstractGameAction.source") return creature(effect.source);
            if (key == "AbstractGameAction.target") return creature(effect.target);
            if (state.fields.contains(py::str(field))) return py::reinterpret_borrow<py::object>(state.fields[py::str(field)]);
            return {};
        });
    }

    py::object pile_group(PileId pile) {
        switch (pile) {
        case PileId::Hand: return group(3, "HAND", &game.hand);
        case PileId::Draw: return group(1, "DRAW_PILE", &game.draw);
        case PileId::Discard: return group(0, "DISCARD_PILE", &game.discard);
        case PileId::Exhaust: return group(2, "EXHAUST_PILE", &game.exhaust);
        case PileId::Limbo: return group(4, "UNSPECIFIED", &game.limbo);
        case PileId::Grid: return group(7, "UNSPECIFIED", &game.grid_cards);
        }
        SPIRE_ASSERT(false);
        __builtin_unreachable();
    }

    py::object effects(const Queue<uint8_t, action_capacity>& queue) {
        py::list result;
        for (int i = 0; i < queue.size; ++i) {
            py::object item = effect(game.action_objects[queue[i]], 3, queue[i]);
            if (!item) return {};
            result.append(item);
        }
        return result;
    }

    py::object manager() {
        const ActionManager& manager = game.manager;
        return object({14, 0, 0}, "actions.GameActionManager", [&](const std::string& key) -> py::object {
            const std::string field = key.substr(key.find('.') + 1);
            if (field == "actions") return effects(manager.actions);
            if (field == "preTurnActions") return effects(manager.pre_turn_actions);
            if (field == "cardQueue") {
                py::list result;
                for (int i = 0; i < manager.card_queue.size; ++i) {
                    const CardQueueItem& item = manager.card_queue[i];
                    result.append(object({18, i, 0}, "cards.CardQueueItem", [&](const std::string& key) -> py::object {
                        const std::string field = key.substr(key.find('.') + 1);
                        if (field == "card") return card(item.card);
                        if (field == "monster") return item.target < 0 ? py::object(py::none()) : monster(item.target);
                        if (field == "energyOnUse") return py::int_(item.energy_on_use);
                        if (field == "autoplayCard") return py::bool_(item.autoplay_card);
                        if (field == "ignoreEnergyTotal") return py::bool_(item.ignore_energy_total);
                        if (field == "isEndTurnAutoPlay") return py::bool_(item.is_end_turn_auto_play);
                        if (field == "randomTarget") return py::bool_(item.random_target);
                        return {};
                    }));
                }
                return result;
            }
            if (field == "monsterQueue") {
                py::list result;
                for (int i = 0; i < manager.monster_queue.size; ++i)
                    result.append(object({19, i, 0}, "monsters.MonsterQueueItem", [&](const std::string& key) -> py::object {
                        if (key == "MonsterQueueItem.monster") return monster(manager.monster_queue[i]);
                        return {};
                    }));
                return result;
            }
            if (field == "cardsPlayedThisCombat" || field == "cardsPlayedThisTurn") {
                py::list result;
                const int first = field == "cardsPlayedThisCombat" ? 0 : manager.played_cards.turn_start;
                for (int i = first; i < manager.played_cards.size; ++i) result.append(card(manager.played_cards.values[i]));
                return result;
            }
            if (field == "currentAction") return manager.has_current_action ? effect(game.action_objects[manager.current_action], 3, manager.current_action) : py::object(py::none());
            if (field == "hasControl") return py::bool_(manager.has_current_action);
            if (field == "lastCard" || field == "turnStartCurrentAction") return py::none();
            if (field == "usingCard") return py::bool_(false);
            if (field == "mantraGained") return py::int_(0);
            if (field == "monsterAttacksQueued") return py::bool_(manager.monster_attacks_queued);
            if (field == "nextCombatActions" || field == "orbsChanneledThisCombat" || field == "orbsChanneledThisTurn") return py::list();
            if (field == "phase") return py::str(manager.phase == Phase::WaitingOnUser ? "WAITING_ON_USER" : "EXECUTING_ACTIONS");
            if (field == "turnHasEnded") return py::bool_(manager.turn_has_ended);
            if (field == "uniqueStancesThisCombat") return py::dict();
            return {};
        });
    }

    py::dict combat() const {
        const ActionManager& manager = game.manager;
        return py::dict("totalDiscardedThisTurn"_a=manager.total_discarded_this_turn,
            "damageReceivedThisTurn"_a=manager.damage_received_this_turn,
            "damageReceivedThisCombat"_a=manager.damage_received_this_combat,
            "hpLossThisCombat"_a=manager.hp_loss_this_combat, "playerHpLastTurn"_a=manager.player_hp_last_turn,
            "energyGainedThisCombat"_a=manager.energy_gained_this_combat, "turn"_a=game.turn, "energy"_a=game.player.energy);
    }

    py::dict screen(const FreshRunState& fresh) {
        const bool selecting = game.screen == Screen::Discard || game.screen == Screen::Grid;
        const bool grid = game.screen == Screen::Grid;
        const bool card_reward = game.screen == Screen::CardReward;
        const bool rewards = game.screen == Screen::Victory;
        py::dict result("type"_a=(selecting ? (grid ? "GRID" : "HAND_SELECT") : card_reward ? "CARD_REWARD" : game.screen == Screen::Death ? "DEATH" : rewards ? "COMBAT_REWARD" : "NONE"),
            "up"_a=(selecting || card_reward || game.screen == Screen::Death || rewards), "previous"_a=py::none(),
            "choice"_a=(selecting ? (grid ? "GRID" : "HAND_SELECT") : card_reward ? "CARD_REWARD" : game.screen == Screen::Death ? "DEATH" : rewards ? "COMBAT_REWARD" : "NONE"));
        if (rewards) { result["options"] = game.smoked ? py::object(py::list()) : py::cast(fresh.rewardTypes); result["state"] = reward_screen(fresh); }
        else if (card_reward) {
            py::list options;
            for (int i = 0; i < game.reward_cards.size; ++i)
                options.append(card_names[static_cast<int>(game.cards[game.reward_cards[i]].id)]);
            result["options"] = options;
            result["state"] = object({26, 0, 0}, "screens.CardRewardScreen", [&](const std::string& key) -> py::object {
                const std::string field = key.substr(key.find('.') + 1);
                if (field == "rewardGroup") return cards(game.reward_cards);
                if (field == "discoveryCard") return card(game.discovery_card);
                if (field == "discovery") return py::bool_(true);
                if (field == "skippable") return py::bool_(game.reward_skippable);
                if (field == "draftCount") return py::int_(0);
                if (field == "codexCard" || field == "rItem") return py::none();
                if (field == "cardOnly" || field == "chooseOne" || field == "codex" || field == "draft" || field == "hasTakenAll") return py::bool_(false);
                return {};
            });
        }
        else if (!selecting) { result["options"] = py::list(); result["state"] = py::none(); }
        else {
            py::list options;
            const Pile& choices = grid ? get_pile(game, game.grid_source) : game.hand;
            if (grid || game.selected_cards.size < game.selection_amount)
                for (int i = 0; i < choices.size; ++i)
                    options.append(card_names[static_cast<int>(game.cards[choices[i]].id)]);
            result["options"] = options;
            result["state"] = object({17, grid ? 1 : 0, 0}, grid ? "screens.select.GridCardSelectScreen" : "screens.select.HandCardSelectScreen", [&](const std::string& key) -> py::object {
                const std::string field = key.substr(key.find('.') + 1);
                if (grid) {
                    if (field == "targetGroup") return pile_group(game.grid_source);
                    if (field == "selectedCards") {
                        py::list selected;
                        for (int i = 0; i < game.selected_cards.size; ++i) selected.append(card(game.selected_cards[i]));
                        return selected;
                    }
                    if (field == "numCards") return py::int_(game.selection_amount);
                    if (field == "cardSelectAmount") return py::int_(game.selected_cards.size);
                    if (field == "prevDeckSize") return py::int_(choices.size);
                    if (field == "hoveredCard") return py::none();
                    if (field == "anyNumber") return py::bool_(game.selection_any_number);
                    if (field == "canCancel") return py::bool_(true);
                    if (field == "forUpgrade" || field == "forTransform" || field == "forPurge" || field == "confirmScreenUp" ||
                        field == "isJustForConfirming" || field == "cancelWasOn" || field == "forClarity") return py::bool_(false);
                    return {};
                }
                if (field == "numCardsToSelect") return py::int_(game.selection_amount);
                if (field == "selectedCards") return group(6, "UNSPECIFIED", &game.selected_cards);
                if (field == "hand") return group(3, "HAND", &game.hand);
                if (field == "wereCardsRetrieved") return py::bool_(game.selected_cards_retrieved);
                if (field == "numSelected") return py::int_(0);
                if (field == "forUpgrade") return py::bool_(game.selection_for_upgrade);
                if (field == "anyNumber") return py::bool_(game.selection_any_number);
                if (field == "canPickZero") return py::bool_(game.selection_can_pick_zero);
                if (field == "upTo") return py::bool_(game.selection_up_to);
                if (field == "forTransform") return py::bool_(false);
                return {};
            });
        }
        result["room_dialog"] = py::dict("optionList"_a=py::list(), "selectedOption"_a=-1, "waitForInput"_a=true);
        result["image_dialog"] = py::dict("selectedOption"_a=-1, "waitForInput"_a=true);
        return result;
    }

    py::dict globals(const FreshRunState& fresh) {
        py::dict result("event_chances"_a=py::dict("ELITE_CHANCE"_a=0.0, "MONSTER_CHANCE"_a=0.1, "SHOP_CHANCE"_a=0.03, "TREASURE_CHANCE"_a=0.02),
            "potion"_a=py::dict("blizzardPotionMod"_a=fresh.blizzardPotionMod), "shop"_a=py::dict("purgeCost"_a=75, "actualPurgeCost"_a=75),
            "drawn_cards"_a=cards(game.drawn_cards), "scraped_cards"_a=py::list(), "obtained_cards"_a=py::dict());
        for (auto item : inventory["globals"].cast<py::dict>()) result[item.first] = item.second;
        snapshot_statics(*this, result);
        py::dict obtained;
        for (int i = 0; i < static_cast<int>(CardId::Count); ++i)
            if (game.obtained_cards[i] > 0) obtained[py::str(card_names[i])] = game.obtained_cards[i];
        result["obtained_cards"] = obtained;
        return result;
    }

    py::dict run(const FreshRunState& fresh) const {
        py::dict result("id"_a="Exordium", "floorNum"_a=game.run.floor, "actNum"_a=game.run.act,
            "ascensionLevel"_a=game.run.ascension, "isAscensionMode"_a=true,
            "isDungeonBeaten"_a=false, "is_victory"_a=false, "ascensionCheck"_a=true,
            "bossKey"_a=fresh.bossKey, "bossCount"_a=0, "firstRoomChosen"_a=false,
            "cardBlizzRandomizer"_a=fresh.cardBlizzRandomizer, "cardBlizzStartOffset"_a=5, "cardBlizzGrowth"_a=1, "cardBlizzMaxOffset"_a=-40,
            "shrineChance"_a=0.25, "cardUpgradedChance"_a=0.0, "colorlessRareChance"_a=0.3,
            "shopRoomChance"_a=0.05, "restRoomChance"_a=0.12, "eventRoomChance"_a=0.22,
            "eliteRoomChance"_a=0.08, "treasureRoomChance"_a=0.0,
            "smallChestChance"_a=50, "mediumChestChance"_a=33, "largeChestChance"_a=17,
            "commonRelicChance"_a=50, "uncommonRelicChance"_a=33, "rareRelicChance"_a=17,
            "pathX"_a=py::list(), "pathY"_a=py::list(), "seed"_a=signed_long(game.run.seed),
            "lastCombatMetricKey"_a=encounter_names[static_cast<int>(game.encounter)]);
        result["keys"] = py::cast(std::array<bool, 3>{game.run.ruby_key, game.run.emerald_key, game.run.sapphire_key});
        result["monsterList"] = py::cast(fresh.monsterList);
        result["eliteMonsterList"] = py::cast(fresh.eliteMonsterList);
        result["bossList"] = py::cast(fresh.bossList);
        result["eventList"] = py::cast(fresh.eventList);
        result["shrineList"] = py::cast(fresh.shrineList);
        result["specialOneTimeEventList"] = py::cast(fresh.specialOneTimeEventList);
        result["commonRelicPool"] = py::cast(fresh.commonRelicPool);
        result["uncommonRelicPool"] = py::cast(fresh.uncommonRelicPool);
        result["rareRelicPool"] = py::cast(fresh.rareRelicPool);
        result["shopRelicPool"] = py::cast(fresh.shopRelicPool);
        result["bossRelicPool"] = py::cast(fresh.bossRelicPool);
        if (!fresh.relicsToRemoveOnStart.empty()) result["relicsToRemoveOnStart"] = py::cast(fresh.relicsToRemoveOnStart);
        return result;
    }

    py::list map(const FreshRunState& fresh) const {
        py::list result;
        for (int y = 0; y < 15; ++y) {
            py::list row;
            for (int x = 0; x < 7; ++x) {
                const auto& node = fresh.map[y][x];
                row.append(py::dict("position"_a=py::cast(std::array<int, 2>{x, y}),
                    "room"_a=(node.room.empty() ? py::object(py::none()) : py::object(py::str(node.room))),
                    "emerald"_a=node.emerald, "taken"_a=node.taken, "edges"_a=node.edges));
            }
            result.append(row);
        }
        return result;
    }

    py::dict rng() const {
        py::dict result;
        const char* names[] = {"monsterRng", "mapRng", "eventRng", "merchantRng", "cardRng", "treasureRng", "relicRng", "potionRng", "monsterHpRng", "aiRng", "shuffleRng", "cardRandomRng", "miscRng"};
        const Random* streams[] = {&game.rng.monster, &game.rng.map, &game.rng.event, &game.rng.merchant, &game.rng.card, &game.rng.treasure, &game.rng.relic, &game.rng.potion, &game.rng.monster_hp, &game.rng.ai, &game.rng.shuffle, &game.rng.card_random, &game.rng.misc};
        for (int i = 0; i < 13; ++i) result[names[i]] = py::cast(std::array<int64_t, 3>{streams[i]->counter, signed_long(streams[i]->generator.s0), signed_long(streams[i]->generator.s1)});
        result["neow"] = game.rng.neow_initialized ? py::cast(std::array<int64_t, 3>{game.rng.neow.counter, signed_long(game.rng.neow.generator.s0), signed_long(game.rng.neow.generator.s1)}) : py::object(py::none());
        result["unseeded"] = py::cast(std::array<int64_t, 3>{game.unseeded.counter, signed_long(game.unseeded.generator.s0), signed_long(game.unseeded.generator.s1)});
        return result;
    }

    template<std::size_t Size>
    void pool(py::dict& pools, const char* name, const CardPoolEntry (&ids)[Size], int source_index) const {
        py::list direct, source;
        const auto spec = [](const char* id) {
            py::list result;
            result.append(id); result.append(0); result.append(0);
            return result;
        };
        for (const CardPoolEntry& card : ids) direct.append(spec(card.id));
        if (source_index < 0) {
            for (std::size_t i = Size; i > 0; --i) source.append(spec(ids[i - 1].id));
        } else {
            const auto& entries = game.run.source_card_pools[source_index];
            for (int i = 0; i < entries.size; ++i) source.append(spec(ids[entries[i]].id));
        }
        pools[py::str(std::string(name) + "CardPool")] = direct;
        std::string capital(name);
        capital[0] -= 'a' - 'A';
        pools[py::str("src" + capital + "CardPool")] = source;
    }

    py::dict capture(const FreshRunState& fresh) {
        py::dict result;
        result["run"] = run(fresh);
        result["player"] = player();
        result["room"] = room(fresh);
        result["map"] = map(fresh);
        result["node"] = py::cast(std::array<int, 2>{0, 0});
        result["next_node"] = py::cast(std::array<int, 2>{0, 0});
        result["manager"] = manager();
        result["combat"] = combat();
        result["screen"] = screen(fresh);
        result["globals"] = globals(fresh);
        py::dict pools;
        pool(pools, "colorless", colorless_card_pool, 3);
        pool(pools, "curse", curse_card_pool, -1);
        pool(pools, "common", common_card_pool, 0);
        pool(pools, "uncommon", uncommon_card_pool, 1);
        pool(pools, "rare", rare_card_pool, 2);
        result["pools"] = pools;
        result["rng"] = rng();
        result["objects"] = objects;
        return result;
    }
};
}
