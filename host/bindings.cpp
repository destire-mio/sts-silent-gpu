#include "batch.hpp"
#include "names.hpp"
#include "core/card_data.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace pybind11::literals;

namespace spire::host {
py::dict card_snapshot(const Card& card) {
    return py::dict(
        "id"_a=card_names[static_cast<int>(card.id)], "upgrades"_a=card.upgrades, "misc"_a=card.misc,
        "cost"_a=card.cost, "cost_for_turn"_a=card.cost_for_turn,
        "base_damage"_a=card.base_damage, "damage"_a=card.damage,
        "base_block"_a=card.base_block, "block"_a=card.block,
        "base_magic"_a=card.base_magic, "magic"_a=card.magic,
        "exhaust"_a=card.exhaust, "ethereal"_a=card.ethereal,
        "retain"_a=card.retain, "self_retain"_a=card.self_retain,
        "innate"_a=card.innate, "free_to_play_once"_a=card.free_to_play_once,
        "exhaust_on_use_once"_a=card.exhaust_on_use_once);
}

py::list pile_snapshot(const Game& game, const Pile& pile) {
    py::list result;
    for (int i = 0; i < pile.size; ++i) {
        py::dict card = card_snapshot(game.cards[pile[i]]);
        card["instance"] = pile[i];
        result.append(card);
    }
    return result;
}

template<class Container>
py::list indices(const Container& values) {
    py::list result;
    for (int i = 0; i < values.size; ++i) result.append(values[i]);
    return result;
}

py::list powers_snapshot(const Powers& powers) {
    py::list result;
    for (int i = 0; i < powers.size; ++i) {
        const Power& power = powers[i];
        result.append(py::dict("id"_a=power_names[static_cast<int>(power.id)],
            "amount"_a=power.amount, "just_applied"_a=power.just_applied,
            "triggered"_a=power.triggered, "skip_first"_a=power.skip_first));
    }
    return result;
}

py::dict creature_snapshot(const Creature& creature) {
    return py::dict("hp"_a=creature.hp, "max_hp"_a=creature.max_hp,
        "block"_a=creature.block, "last_damage_taken"_a=creature.last_damage_taken,
        "powers"_a=powers_snapshot(creature.powers), "dead"_a=creature.dead,
        "dying"_a=creature.dying, "escaped"_a=creature.escaped, "half_dead"_a=creature.half_dead);
}

py::dict monster_snapshot(const Monster& monster, int instance) {
    py::dict result = creature_snapshot(monster.creature);
    result["id"] = monster_names[static_cast<int>(monster.id)];
    result["instance"] = instance;
    result["intent"] = intent_names[static_cast<int>(monster.intent)];
    result["move"] = monster.move;
    result["intent_base_damage"] = monster.intent_base_damage;
    result["intent_damage"] = monster.intent_damage;
    result["intent_hits"] = monster.intent_hits;
    result["pending_move"] = monster.pending_move;
    result["pending_intent"] = intent_names[static_cast<int>(monster.pending_intent)];
    result["pending_base_damage"] = monster.pending_base_damage;
    result["pending_hits"] = monster.pending_hits;
    result["move_history"] = indices(monster.move_history);
    result["damage"] = py::cast(std::vector<int>(monster.damage, monster.damage + 3));
    result["block_amount"] = monster.block_amount;
    result["magic"] = monster.magic;
    result["turn_count"] = monster.turn_count;
    result["stolen_gold"] = monster.stolen_gold;
    result["first_move"] = monster.first_move;
    result["used_special"] = monster.used_special;
    result["split_triggered"] = monster.split_triggered;
    return result;
}

py::dict action_snapshot(const Action& action) {
    switch (action.kind) {
    case ActionKind::Play: return py::dict("type"_a="play", "card"_a=action.index, "target"_a=action.target);
    case ActionKind::EndTurn: return py::dict("type"_a="end_turn");
    case ActionKind::Choose: return py::dict("type"_a="choose", "option"_a=action.index);
    case ActionKind::PotionUse: return py::dict("type"_a="potion_use", "slot"_a=action.index, "target"_a=action.target);
    case ActionKind::PotionDiscard: return py::dict("type"_a="potion_discard", "slot"_a=action.index);
    case ActionKind::Proceed: return py::dict("type"_a="proceed");
    case ActionKind::Skip: return py::dict("type"_a="skip");
    }
    SPIRE_ASSERT(false);
}

Action parse_action(const py::dict& value) {
    const std::string type = value["type"].cast<std::string>();
    if (type == "play") return {ActionKind::Play, value["card"].cast<int>(), value["target"].cast<int>()};
    if (type == "end_turn") return {ActionKind::EndTurn};
    if (type == "choose") return {ActionKind::Choose, value["option"].cast<int>()};
    if (type == "potion_use") return {ActionKind::PotionUse, value["slot"].cast<int>(), value["target"].cast<int>()};
    if (type == "potion_discard") return {ActionKind::PotionDiscard, value["slot"].cast<int>()};
    if (type == "proceed") return {ActionKind::Proceed};
    if (type == "skip") return {ActionKind::Skip};
    throw std::invalid_argument("Unknown action: " + type);
}

py::list legal_snapshot(const Game& game) {
    ActionSet actions;
    legal_actions(game, actions);
    py::list result;
    for (int i = 0; i < actions.size; ++i) result.append(action_snapshot(actions[i]));
    return result;
}

py::dict effect_snapshot(const Effect& effect) {
    return py::dict("type"_a=effect_names[static_cast<int>(effect.kind)],
        "source"_a=effect.source, "target"_a=effect.target, "amount"_a=effect.amount,
        "card"_a=effect.card, "power"_a=power_names[static_cast<int>(effect.power)],
        "damage_type"_a=damage_type_names[static_cast<int>(effect.damage_type)],
        "extra"_a=effect.extra, "flag"_a=effect.flag);
}

py::list effect_queue(const Queue<Effect, action_capacity>& queue) {
    py::list result;
    for (int i = 0; i < queue.size; ++i) result.append(effect_snapshot(queue[i]));
    return result;
}

py::dict manager_snapshot(const ActionManager& manager) {
    py::list card_queue;
    for (int i = 0; i < manager.card_queue.size; ++i)
        card_queue.append(py::dict("card"_a=manager.card_queue[i].card, "target"_a=manager.card_queue[i].target));
    return py::dict("actions"_a=effect_queue(manager.actions),
        "pre_turn_actions"_a=effect_queue(manager.pre_turn_actions), "card_queue"_a=card_queue,
        "monster_queue"_a=indices(manager.monster_queue),
        "cards_played_this_turn"_a=indices(manager.cards_played_this_turn),
        "cards_played_this_combat"_a=indices(manager.cards_played_this_combat),
        "phase"_a=(manager.phase == Phase::WaitingOnUser ? "WAITING_ON_USER" : "EXECUTING_ACTIONS"),
        "turn_has_ended"_a=manager.turn_has_ended,
        "monster_attacks_queued"_a=manager.monster_attacks_queued,
        "total_discarded_this_turn"_a=manager.total_discarded_this_turn,
        "damage_received_this_turn"_a=manager.damage_received_this_turn,
        "damage_received_this_combat"_a=manager.damage_received_this_combat,
        "hp_loss_this_combat"_a=manager.hp_loss_this_combat,
        "energy_gained_this_combat"_a=manager.energy_gained_this_combat,
        "player_hp_last_turn"_a=manager.player_hp_last_turn);
}

py::dict random_snapshot(const Random& random) {
    return py::dict("counter"_a=random.counter,
        "state0"_a=signed_long(random.generator.s0), "state1"_a=signed_long(random.generator.s1));
}

py::dict snapshot(const Game& game) {
    py::list deck;
    for (int i = 0; i < game.run.master_deck.size; ++i) {
        const Card& card = game.run.master_deck[i];
        deck.append(py::dict("id"_a=card_names[static_cast<int>(card.id)], "upgrades"_a=card.upgrades, "misc"_a=card.misc));
    }
    py::list relics;
    for (int i = 0; i < game.run.relics.size; ++i)
        relics.append(py::dict("id"_a=relic_names[static_cast<int>(game.run.relics[i].id)], "counter"_a=game.run.relics[i].counter));
    py::list potions;
    for (const PotionId potion : game.run.potions)
        potions.append(potion == PotionId::Empty ? py::none() : py::cast(potion_names[static_cast<int>(potion)]));
    py::dict run("seed"_a=signed_long(game.run.seed), "floor"_a=game.run.floor,
        "act"_a=game.run.act, "ascension"_a=game.run.ascension,
        "hp"_a=game.player.creature.hp, "max_hp"_a=game.player.creature.max_hp,
        "gold"_a=game.run.gold, "keys"_a=py::dict("ruby"_a=game.run.ruby_key,
            "emerald"_a=game.run.emerald_key, "sapphire"_a=game.run.sapphire_key),
        "master_deck"_a=deck, "relics"_a=relics, "potions"_a=potions);
    py::dict player = creature_snapshot(game.player.creature);
    player["energy"] = game.player.energy;
    player["energy_master"] = game.player.energy_master;
    player["hand_size"] = game.player.hand_size;
    player["card_in_use"] = game.player.card_in_use;
    player["cards_played_this_turn"] = game.player.cards_played_this_turn;
    player["damaged_this_combat"] = game.player.damaged_this_combat;
    py::list monsters;
    for (int i = 0; i < game.monster_order.size; ++i) {
        const int index = game.monster_order[i];
        monsters.append(monster_snapshot(game.monsters[index], index));
    }
    py::list registry;
    for (int i = 0; i < game.cards.size; ++i) registry.append(card_snapshot(game.cards[i]));
    py::dict combat("encounter"_a=encounter_names[static_cast<int>(game.encounter)],
        "turn"_a=game.turn, "battle_over"_a=game.battle_over, "mugged"_a=game.mugged,
        "discard_remaining"_a=game.discard_remaining, "player"_a=player,
        "hand"_a=pile_snapshot(game, game.hand), "draw"_a=pile_snapshot(game, game.draw),
        "discard"_a=pile_snapshot(game, game.discard), "exhaust"_a=pile_snapshot(game, game.exhaust),
        "retained"_a=pile_snapshot(game, game.retained), "drawn_cards"_a=indices(game.drawn_cards), "cards"_a=registry,
        "monsters"_a=monsters, "manager"_a=manager_snapshot(game.manager));
    py::dict rng("monster"_a=random_snapshot(game.rng.monster), "map"_a=random_snapshot(game.rng.map),
        "event"_a=random_snapshot(game.rng.event), "merchant"_a=random_snapshot(game.rng.merchant),
        "card"_a=random_snapshot(game.rng.card), "treasure"_a=random_snapshot(game.rng.treasure),
        "relic"_a=random_snapshot(game.rng.relic), "potion"_a=random_snapshot(game.rng.potion),
        "monster_hp"_a=random_snapshot(game.rng.monster_hp), "ai"_a=random_snapshot(game.rng.ai),
        "shuffle"_a=random_snapshot(game.rng.shuffle), "card_random"_a=random_snapshot(game.rng.card_random),
        "misc"_a=random_snapshot(game.rng.misc), "unseeded"_a=py::dict("state"_a=game.unseeded.state));
    return py::dict("run"_a=run,
        "screen"_a=py::dict("type"_a=screen_names[static_cast<int>(game.screen)], "options"_a=legal_snapshot(game)),
        "combat"_a=combat, "rng"_a=rng);
}

CombatSetup parse_setup(const Game& game, const py::dict& value) {
    CombatSetup setup;
    setup.encounter = parse_id<EncounterId>(value["encounter"].cast<std::string>(), encounter_names);
    setup.hp = game.player.creature.hp;
    setup.max_hp = game.player.creature.max_hp;
    setup.gold = game.run.gold;
    setup.deck = game.run.master_deck;
    setup.relics = game.run.relics;
    if (value.contains("unseeded_seed")) setup.unseeded_seed = value["unseeded_seed"].cast<std::int64_t>();
    if (value.contains("floor")) setup.floor = value["floor"].cast<int>();
    if (value.contains("hp")) setup.hp = value["hp"].cast<int>();
    if (value.contains("max_hp")) setup.max_hp = value["max_hp"].cast<int>();
    if (value.contains("gold")) setup.gold = value["gold"].cast<int>();
    if (value.contains("deck")) {
        setup.deck.clear();
        for (py::handle item : value["deck"].cast<py::list>()) {
            const py::dict card = py::reinterpret_borrow<py::dict>(item);
            setup.deck.push_back(make_card(parse_id<CardId>(card["id"].cast<std::string>(), card_names),
                card.contains("upgrades") ? card["upgrades"].cast<int>() : 0,
                card.contains("misc") ? card["misc"].cast<int>() : 0));
        }
    }
    if (value.contains("relics")) {
        setup.relics.clear();
        for (py::handle item : value["relics"].cast<py::list>()) {
            setup.relics.push_back({parse_id<RelicId>(py::cast<std::string>(item), relic_names), -1});
        }
    }
    if (value.contains("potions")) {
        const py::list potions = value["potions"].cast<py::list>();
        SPIRE_ASSERT(potions.size() <= 2);
        for (py::ssize_t i = 0; i < potions.size(); ++i) {
            if (!potions[i].is_none())
                throw std::invalid_argument("Unsupported potion: " + potions[i].cast<std::string>());
        }
    }
    return setup;
}

void start_scenario(Game& game, std::int64_t seed, const py::dict& scenario) {
    reset(game, seed);
    start_combat(game, parse_setup(game, scenario));
}

void python_step(Game& game, const py::dict& action) { step(game, parse_action(action)); }
}

PYBIND11_MODULE(spire, module) {
    using namespace spire;
    using namespace spire::host;
    py::class_<Game>(module, "Game")
        .def(py::init<>())
        .def("reset", [](Game& game, std::int64_t seed) { reset(game, seed); }, "seed"_a)
        .def("start_combat", &start_scenario, "seed"_a, "scenario"_a)
        .def("legal_actions", &legal_snapshot)
        .def("step", &python_step, "action"_a)
        .def("snapshot", &snapshot);
    module.def("reset", [](std::int64_t seed) { Game game; reset(game, seed); return game; }, "seed"_a);
    module.def("start_combat", [](std::int64_t seed, const py::dict& scenario) {
        Game game; start_scenario(game, seed, scenario); return game;
    }, "seed"_a, "scenario"_a);
    module.def("legal_actions", &legal_snapshot, "game"_a);
    module.def("step", &python_step, "game"_a, "action"_a);
    module.def("snapshot", &snapshot, "game"_a);
    py::class_<Batch>(module, "Batch")
        .def(py::init<std::size_t>(), "size"_a)
        .def("reset", &Batch::reset, "seeds"_a)
        .def("start_combat", [](Batch& batch, const std::vector<std::int64_t>& seeds, const py::dict& scenario) {
            SPIRE_ASSERT(seeds.size() == batch.games.size());
            for (std::size_t i = 0; i < batch.games.size(); ++i) start_scenario(batch.games[i], seeds[i], scenario);
        }, "seeds"_a, "scenario"_a)
        .def("legal_actions", [](const Batch& batch) {
            py::list result;
            for (const Game& game : batch.games) result.append(legal_snapshot(game));
            return result;
        })
        .def("step", [](Batch& batch, const py::list& values) {
            std::vector<Action> actions;
            actions.reserve(values.size());
            for (py::handle value : values) actions.push_back(parse_action(py::reinterpret_borrow<py::dict>(value)));
            batch.step(actions);
        }, "actions"_a)
        .def("snapshot", [](const Batch& batch) {
            py::list result;
            for (const Game& game : batch.games) result.append(snapshot(game));
            return result;
        })
        .def("__len__", [](const Batch& batch) { return batch.games.size(); });
    module.attr("encounters") = py::cast(std::vector<std::string>(std::begin(encounter_names), std::end(encounter_names)));
}
