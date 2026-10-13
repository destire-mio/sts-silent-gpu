#pragma once
#include "names.hpp"
#include "run_state.hpp"
#include "card_pool_data.hpp"
#include "core/card_data.hpp"
#include "spire_host_generated/snapshot.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <array>
#include <charconv>
#include <map>
#include <set>
#include <string>
#include <type_traits>

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
        if constexpr (std::is_enum_v<T>) fields[py::str(field)] = py::cast(static_cast<std::underlying_type_t<T>>(value));
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
            if (field == "isInAutoplay") return py::bool_(card.in_autoplay);
            if (field == "dontTriggerOnUseCard") return py::bool_(card.dont_trigger_on_use_card);
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
                field == "isSelected" || field == "isUsed" || field == "returnToHand" ||
                field == "shuffleBackIntoDrawPile" || field == "upgradedCost") return py::bool_(false);
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

    py::object creature(int owner) { return owner == player_target ? player() : monster(owner); }

    py::object power_value(const Power& power, int owner, std::array<int, 3> identity) {
        const PowerData data = power_data(power.id);
        UnitStateSnapshot state;
        power_visit_state(power, state);
        if (power.id_offset >= 0) identity = {25, owner, power.id_offset};
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
            if (field == "isTurnBased") return py::bool_(data.turn_based || power.id == PowerId::Weak || power.id == PowerId::Vulnerable || power.id == PowerId::Frail || power.id == PowerId::PenNib || power.id == PowerId::Poison || power.id == PowerId::NoBlock);
            if (field == "isPostActionPower") return py::bool_(power.id == PowerId::Angry || power.id == PowerId::Shifting);
            if (key == "ConstrictedPower.source") {
                const int source = power.state.Constricted.source;
                return source == no_target ? py::object(py::none()) : creature(source);
            }
            if (field == "justApplied") return py::bool_(power.just_applied);
            if (field == "triggered") return py::bool_(power.triggered);
            if (field == "skipFirst") return py::bool_(power.skip_first);
            if (field == "onPlayer") return py::bool_(owner == player_target);
            if (key == "PoisonPower.source") {
                const int source = power.state.Poison.source;
                return source == no_target ? py::object(py::none()) : creature(source);
            }
            if (key == "NightmarePower.card") return card(power.state.Nightmare.card);
            if (key == "StasisPower.card") return card(power.state.Stasis.card);
            if (state.fields.contains(py::str(field))) return py::reinterpret_borrow<py::object>(state.fields[py::str(field)]);
            return {};
        });
    }

    py::object power(int owner, int index) {
        return power_value(get_creature(game, owner).powers[index], owner, {6, owner, index});
    }

    py::object creature_field(int owner, const std::string& key) {
        const Creature& c = get_creature(game, owner);
        if (key == "AbstractCreature.currentBlock") return py::int_(c.block);
        if (key == "AbstractCreature.currentHealth") return py::int_(c.hp);
        if (key == "AbstractCreature.drawX") {
            if (owner != player_target && c.escaped) return py::none();
            return java_float(owner == player_target ? 320.0f : c.draw_x);
        }
        if (key == "AbstractCreature.flipHorizontal") return py::bool_(c.flip_horizontal);
        if (key == "AbstractCreature.gold") return py::int_(owner == player_target ? game.run.gold : c.gold);
        if (key == "AbstractCreature.halfDead") return py::bool_(c.half_dead);
        if (key == "AbstractCreature.id") return owner == player_target ? py::object(py::none()) : py::object(py::str(monster_names[static_cast<int>(game.monsters[owner].id)]));
        if (key == "AbstractCreature.isBloodied") return py::bool_(owner == player_target && game.player.bloodied);
        if (key == "AbstractCreature.isDead") return py::bool_(c.dead);
        if (key == "AbstractCreature.isDying") return py::bool_(c.dying);
        if (key == "AbstractCreature.isEscaping") return py::bool_(c.escaped);
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
        SPIRE_ASSERT(id == PotionId::Empty);
        return object({10, index, 0}, class_name("potions", static_cast<int>(id)), [&](const std::string& key) -> py::object {
            if (key == "AbstractPotion.ID") return py::str("Potion Slot");
            if (key == "AbstractPotion.potency") return py::int_(0);
            if (key == "AbstractPotion.slot") return py::int_(-1);
            if (key == "AbstractPotion.rarity") return py::str("PLACEHOLDER");
            if (key == "AbstractPotion.size") return py::str("T");
            if (key == "AbstractPotion.canUse" || key == "AbstractPotion.isThrown" || key == "AbstractPotion.targetRequired") return py::bool_(false);
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
            if (field == "isMultiDmg") return py::bool_(monster.intent_hits > 1);
            if (field == "moveHistory") {
                py::list result;
                for (int i = 0; i < monster.move_history.size; ++i) result.append(monster.move_history[i]);
                return result;
            }
            if (field == "nextMove") return py::int_(monster.move);
            if (field == "type") return py::str("NORMAL");
            if (field == "move") return object({8, index, 0}, "monsters.EnemyMoveInfo", [&](const std::string& name) -> py::object {
                if (name == "EnemyMoveInfo.baseDamage") return py::int_(monster.pending_base_damage);
                if (name == "EnemyMoveInfo.intent") return py::str(intent_names[static_cast<int>(monster.pending_intent)]);
                if (name == "EnemyMoveInfo.isMultiDamage") return py::bool_(monster.pending_hits > 1);
                if (name == "EnemyMoveInfo.multiplier") return py::int_(monster.pending_hits > 1 ? monster.pending_hits : 0);
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
            if (key == "CombatRewardScreen.smoke") return py::bool_(false);
            if (key == "CombatRewardScreen.hasTakenAll") return py::bool_(fresh.rewardTypes.empty());
            if (key == "CombatRewardScreen.rewards") {
                py::list result;
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
            if (field == "cannotLose" || field == "combatEvent" || field == "eliteTrigger" || field == "skipMonsterTurn" || field == "smoked") return py::bool_(false);
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

    py::object effect(const Effect& effect, int queue, int index) {
        const std::string name = effect_names[static_cast<int>(effect.kind)];
        if (name.compare(0, 8, "actions.") != 0) return {};
        UnitStateSnapshot state;
        action_visit_state(effect, state);
        return object({15, queue, index}, name, [&](const std::string& key) -> py::object {
            const std::string field = key.substr(key.find('.') + 1);
            if (key == "AbstractGameAction.actionType") {
                if (effect.kind == EffectKind::Damage) return py::str("DAMAGE");
                if (effect.kind == EffectKind::Block) return py::str("BLOCK");
                if (effect.kind == EffectKind::Draw) return py::str("DRAW");
                if (effect.kind == EffectKind::Discard) return py::str("DISCARD");
                if (effect.kind == EffectKind::UseCard) return py::str(effect.flag ? "EXHAUST" : "USE");
                if (effect.kind == EffectKind::ApplyPower) return py::str("POWER");
                if (effect.kind == EffectKind::ReducePower) return py::str("REDUCE_POWER");
                if (effect.kind == EffectKind::RemovePower) return py::str("DEBUFF");
                if (effect.kind == EffectKind::Shuffle) return py::str("SHUFFLE");
                if (effect.kind == EffectKind::AddCardDiscard) return py::str("CARD_MANIPULATION");
                return py::none();
            }
            if (key == "AbstractGameAction.amount") {
                if (effect.kind == EffectKind::SetMove || effect.kind == EffectKind::AddCardDiscard || effect.kind == EffectKind::RemovePower) return py::int_(0);
                return py::int_(effect.kind == EffectKind::UseCard ? 1 : effect.amount);
            }
            if (key == "AbstractGameAction.damageType") return py::none();
            if (key == "AbstractGameAction.isDone") return py::bool_(effect.done);
            if (key == "AbstractGameAction.source") {
                if (effect.kind == EffectKind::UseCard || effect.kind == EffectKind::HandCheck || effect.kind == EffectKind::RollMove ||
                    effect.kind == EffectKind::SetMove || effect.kind == EffectKind::AddCardDiscard) return py::none();
                return effect.source == no_target ? py::object(py::none()) : creature(effect.source);
            }
            if (key == "AbstractGameAction.target") {
                if (effect.kind == EffectKind::UseCard) return player();
                if (effect.kind == EffectKind::HandCheck || effect.kind == EffectKind::RollMove ||
                    effect.kind == EffectKind::SetMove || effect.kind == EffectKind::AddCardDiscard) return py::none();
                return effect.target == no_target ? py::object(py::none()) : creature(effect.target);
            }
            if (key == "DamageAction.goldAmount") return py::int_(effect.extra);
            if (key == "DamageAction.muteSfx") return py::bool_(false);
            if (key == "DamageAction.skipWait") return py::bool_(effect.flag);
            if (key == "DamageAction.info") {
                if (effect.source >= 0 && effect.damage_type == DamageType::Normal) {
                    const Monster& owner = game.monsters[effect.source];
                    for (int i = 0; i < owner.damage_count; ++i)
                        if (owner.damage[i] == effect.info_base) return monster_damage(effect.source, i);
                }
                return object({16, queue, index}, "cards.DamageInfo", [&](const std::string& name) -> py::object {
                    if (name == "DamageInfo.base") return py::int_(effect.info_base);
                    if (name == "DamageInfo.output") return py::int_(effect.amount);
                    if (name == "DamageInfo.isModified") return py::bool_(false);
                    if (name == "DamageInfo.owner") return effect.source == no_target ? py::object(py::none()) : creature(effect.source);
                    if (name == "DamageInfo.type") return py::str(damage_type_names[static_cast<int>(effect.damage_type)]);
                    return {};
                });
            }
            if (key == "ApplyPowerAction.powerToApply") return power_value(effect.prepared_power, effect.target, {20, queue, index});
            if (key == "ReducePowerAction.powerID" || key == "RemoveSpecificPowerAction.powerToRemove")
                return effect.power_offset >= 0 ? py::object(py::none()) : py::object(py::str(power_names[static_cast<int>(effect.power)]));
            if (key == "ReducePowerAction.powerInstance" || key == "RemoveSpecificPowerAction.powerInstance") {
                if (effect.power_offset < 0) return py::none();
                const int index = find_power(get_creature(game, effect.target), effect.power, effect.power_offset);
                SPIRE_ASSERT(index >= 0);
                return power(effect.target, index);
            }
            if (key == "SetMoveAction.isMultiplier") return py::bool_(false);
            if (key == "SetMoveAction.monster") return monster(effect.target);
            if (key == "SetMoveAction.theMultiplier") return py::int_(0);
            if (key == "SetMoveAction.theNextDamage") return py::int_(effect.card);
            if (key == "SetMoveAction.theNextIntent") return py::str(intent_names[effect.extra]);
            if (key == "SetMoveAction.theNextMove") return py::int_(effect.amount);
            if (key == "MakeTempCardInDiscardAction.c") return card(effect.card);
            if (key == "MakeTempCardInDiscardAction.numCards") return py::int_(effect.amount);
            if (key == "MakeTempCardInDiscardAction.sameUUID") return py::bool_(effect.flag);
            if (key == "DrawCardAction.clearDrawHistory") return py::bool_(!effect.flag);
            if (key == "DrawCardAction.followUpAction") return py::none();
            if (key == "DiscardAction.endTurn") return py::bool_(effect.flag);
            if (key == "DiscardAction.isRandom") return py::bool_(false);
            if (key == "DiscardAction.p" || key == "HandCheckAction.player") return player();
            if (key == "RollMoveAction.monster") return monster(effect.target);
            if (key == "UseCardAction.exhaustCard") return py::bool_(effect.flag);
            if (key == "UseCardAction.reboundCard" || key == "UseCardAction.returnToHand") return py::bool_(false);
            if (key == "UseCardAction.target") return effect.target < 0 ? py::object(py::none()) : monster(effect.target);
            if (key == "UseCardAction.targetCard") return card(effect.card);
            if (state.fields.contains(py::str(field))) return py::reinterpret_borrow<py::object>(state.fields[py::str(field)]);
            return {};
        });
    }

    py::object effects(const Queue<Effect, action_capacity>& queue, int identity) {
        py::list result;
        for (int i = 0; i < queue.size; ++i) {
            py::object item = effect(queue[i], identity, i);
            if (!item) return {};
            result.append(item);
        }
        return result;
    }

    py::object manager() {
        const ActionManager& manager = game.manager;
        return object({14, 0, 0}, "actions.GameActionManager", [&](const std::string& key) -> py::object {
            const std::string field = key.substr(key.find('.') + 1);
            if (field == "actions") return effects(manager.actions, 0);
            if (field == "preTurnActions") return effects(manager.pre_turn_actions, 1);
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
                        if (field == "isEndTurnAutoPlay" || field == "randomTarget") return py::bool_(false);
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
            if (field == "currentAction") return manager.has_current_action ? effect(manager.current_action, 2, 0) : py::object(py::none());
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
        const bool selecting = game.screen == Screen::Discard;
        const bool rewards = game.screen == Screen::Victory;
        py::dict result("type"_a=(selecting ? "HAND_SELECT" : game.screen == Screen::Death ? "DEATH" : rewards ? "COMBAT_REWARD" : "NONE"),
            "up"_a=(selecting || game.screen == Screen::Death || rewards), "previous"_a=py::none(),
            "choice"_a=(selecting ? "HAND_SELECT" : game.screen == Screen::Death ? "DEATH" : rewards ? "COMBAT_REWARD" : "NONE"));
        if (rewards) { result["options"] = py::cast(fresh.rewardTypes); result["state"] = reward_screen(fresh); }
        else if (!selecting) { result["options"] = py::list(); result["state"] = py::none(); }
        else {
            py::list options;
            if (game.selected_cards.size < game.selection_amount)
                for (int i = 0; i < game.hand.size; ++i)
                    options.append(card_names[static_cast<int>(game.cards[game.hand[i]].id)]);
            result["options"] = options;
            result["state"] = object({17, 0, 0}, "screens.select.HandCardSelectScreen", [&](const std::string& key) -> py::object {
                const std::string field = key.substr(key.find('.') + 1);
                if (field == "numCardsToSelect") return py::int_(game.selection_amount);
                if (field == "selectedCards") return group(6, "UNSPECIFIED", &game.selected_cards);
                if (field == "hand") return group(3, "HAND", &game.hand);
                if (field == "wereCardsRetrieved") return py::bool_(game.selected_cards_retrieved);
                if (field == "numSelected") return py::int_(0);
                if (field == "anyNumber" || field == "canPickZero" || field == "forTransform" ||
                    field == "forUpgrade" || field == "upTo") return py::bool_(false);
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
        result["actions.common.ExhaustAction"] = py::dict("numExhausted"_a=0);
        result["actions.common.DiscardAction"] = py::dict("numDiscarded"_a=game.discard_action_num);
        result["actions.common.PutOnBottomOfDeckAction"] = py::dict("numPlaced"_a=0);
        result["actions.common.PutOnDeckAction"] = py::dict("numPlaced"_a=0);
        result["actions.unique.CodexAction"] = py::dict("numPlaced"_a=0);
        result["actions.unique.NightmareAction"] = py::dict("numDiscarded"_a=0);
        result["events.shrines.FaceTrader"] = py::dict("goldReward"_a=0, "damage"_a=0);
        result["powers.TheBombPower"] = py::dict("bombIdOffset"_a=game.bomb_id_offset);
        result["relics.CentennialPuzzle"] = py::dict("usedThisCombat"_a=false);
        result["relics.OrangePellets"] = py::dict("SKILL"_a=false, "POWER"_a=false, "ATTACK"_a=false);
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
    void pool(py::dict& pools, const char* name, const char* const (&ids)[Size]) const {
        py::list direct, source;
        const auto spec = [](const char* id) {
            py::list result;
            result.append(id); result.append(0); result.append(0);
            return result;
        };
        for (const char* id : ids) direct.append(spec(id));
        for (std::size_t i = Size; i > 0; --i) source.append(spec(ids[i - 1]));
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
        pool(pools, "colorless", colorless_card_pool);
        pool(pools, "curse", curse_card_pool);
        pool(pools, "common", common_card_pool);
        pool(pools, "uncommon", uncommon_card_pool);
        pool(pools, "rare", rare_card_pool);
        result["pools"] = pools;
        result["rng"] = rng();
        result["objects"] = objects;
        return result;
    }
};
}
