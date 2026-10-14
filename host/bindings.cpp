#include "batch.hpp"
#include "snapshot.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <type_traits>

namespace py = pybind11;
using namespace pybind11::literals;

namespace spire::host {
static_assert(std::is_trivially_copyable_v<Game>);
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

CombatSetup parse_setup(const Game& game, const py::dict& value) {
    CombatSetup setup;
    setup.encounter = parse_id<EncounterId>(value["encounter"].cast<std::string>(), encounter_names);
    setup.hp = game.player.creature.hp;
    setup.max_hp = game.player.creature.max_hp;
    setup.gold = game.run.gold;
    setup.deck = game.run.master_deck;
    setup.relics = game.run.relics;
    if (value.contains("floor")) setup.floor = value["floor"].cast<int>();
    if (value.contains("hp")) setup.hp = value["hp"].cast<int>();
    if (value.contains("max_hp")) setup.max_hp = value["max_hp"].cast<int>();
    if (value.contains("gold")) setup.gold = value["gold"].cast<int>();
    if (value.contains("deck")) {
        setup.deck.clear();
        for (py::handle item : value["deck"].cast<py::list>()) {
            const py::dict card = py::reinterpret_borrow<py::dict>(item);
            Card value = make_card(parse_id<CardId>(card["id"].cast<std::string>(), card_names),
                card.contains("upgrades") ? card["upgrades"].cast<int>() : 0);
            if (card.contains("misc")) value.misc = card["misc"].cast<int>();
            setup.deck.push_back(value);
        }
    }
    if (value.contains("relics")) {
        setup.relics.clear();
        for (py::handle item : value["relics"].cast<py::list>()) {
            const RelicId id = parse_id<RelicId>(py::cast<std::string>(item), relic_names);
            Relic relic{id, relic_data(id).counter};
            relic_initialize_state(relic);
            setup.relics.push_back(relic);
        }
    }
    if (value.contains("potions")) {
        const py::list potions = value["potions"].cast<py::list>();
        SPIRE_ASSERT(potions.size() <= potion_capacity);
        for (py::ssize_t i = 0; i < potions.size(); ++i) {
            if (!potions[i].is_none())
                setup.potions[i] = parse_id<PotionId>(potions[i].cast<std::string>(), potion_names);
        }
    }
    if (value.contains("start")) {
        const py::dict start = value["start"].cast<py::dict>();
        setup.act = start["act"].cast<int>();
        setup.floor = start["floor"].cast<int>();
        setup.gold = start["gold"].cast<int>();
        const auto node = start["node"].cast<std::array<int, 2>>();
        setup.node_x = node[0]; setup.node_y = node[1];
        const auto keys = start["keys"].cast<std::array<bool, 3>>();
        for (int i = 0; i < 3; ++i) setup.keys[i] = keys[i];
        setup.emerald = start["emerald"].cast<bool>();
        setup.first_room_chosen = true;
        const std::string name = start["room"].cast<std::string>();
        const char* rooms[] = {"MonsterRoom", "MonsterRoomElite", "MonsterRoomBoss", "ShopRoom", "RestRoom", "TreasureRoom", "EventRoom"};
        bool found = false;
        for (int i = 0; i < 7; ++i) if (name == rooms[i]) { setup.room = static_cast<RoomKind>(i); found = true; }
        if (name == "NeowRoom") { setup.room = RoomKind::Neow; found = true; }
        if (name == "TreasureRoomBoss") { setup.room = RoomKind::BossTreasure; found = true; }
        SPIRE_ASSERT(found);
    }
    return setup;
}


struct Environment {
    Game game;
    FreshRunState fresh;

    void reset(std::int64_t seed, bool playtime_at_least_800 = false) {
        RunProfile profile{};
        profile.playtime_at_least_800 = playtime_at_least_800;
        spire::reset(game, seed, profile);
        fresh = FreshRunState{};
        initialize_fresh_run(fresh);
    }
    void start_combat(std::int64_t seed, const py::dict& scenario) {
        RunProfile profile{};
        if (scenario.contains("playtime_at_least_800")) profile.playtime_at_least_800 = scenario["playtime_at_least_800"].cast<bool>();
        if (scenario.contains("profile")) {
            const py::dict value = scenario["profile"].cast<py::dict>();
            profile.neow_spirits = value["neow_spirits"].cast<int>();
            profile.seeded = value["seeded"].cast<bool>();
            const char* keys[] = {"GUARDIAN", "GHOST", "SLIME", "CHAMP", "AUTOMATON", "COLLECTOR", "CROW", "DONUT", "WIZARD"};
            for (auto item : value["bosses_seen"].cast<py::list>()) {
                const auto key = py::cast<std::string>(item);
                int i = 0; while (i < 9 && key != keys[i]) ++i;
                SPIRE_ASSERT(i < 9); profile.bosses_seen |= 1 << i;
            }
        }
        spire::reset(game, seed, profile);
        fresh = FreshRunState{}; initialize_fresh_run(fresh);
        if (scenario.contains("actions") && !scenario.contains("encounter")) return;
        const auto setup = parse_setup(game, scenario);
        spire::start_combat(game, setup);
    }
    py::dict snapshot() const { return SnapshotGraph{game}.capture(fresh); }
    py::list schema_gaps() const {
        SnapshotGraph graph{game};
        graph.capture(fresh);
        return py::cast(std::vector<std::string>(graph.gaps.begin(), graph.gaps.end()));
    }
    py::list legal_actions() const { return legal_snapshot(game); }
    py::dict search_state() const {
        py::list hand, monsters, powers, choices;
        if (game.screen == Screen::Discard || game.screen == Screen::Grid || game.screen == Screen::CardReward) {
            const auto append = [&](const auto& pile) {
                for (int i = 0; i < pile.size; ++i) {
                    const auto& card = get_card(game, game.screen == Screen::Grid && game.grid_source == PileId::MasterDeck ? -pile[i] - 2 : pile[i]);
                    choices.append(card_names[static_cast<int>(card.id)]);
                }
            };
            if (game.screen == Screen::Discard) append(game.hand);
            else if (game.screen == Screen::Grid) append(get_pile(game, game.grid_source));
            else append(game.reward_cards);
        }
        for (int i = 0; i < game.player.creature.powers.size; ++i) {
            const auto& power = game.player.creature.powers.at(game.power_objects, i);
            powers.append(py::make_tuple(power_names[static_cast<int>(power.id)], power.amount));
        }
        for (int i = 0; i < game.hand.size; ++i) {
            const auto& card = game.cards[game.hand[i]];
            hand.append(py::dict("id"_a=card_names[static_cast<int>(card.id)],
                "type"_a=static_cast<int>(card_data(card.id).type), "cost"_a=card.cost_for_turn,
                "damage"_a=card.damage, "block"_a=card.block, "magic"_a=card.magic));
        }
        for (int index = 0; index < game.monster_order.size; ++index) {
            const auto& monster = game.monsters[game.monster_order[index]];
            py::list powers;
            for (int i = 0; i < monster.creature.powers.size; ++i) {
                const auto& power = monster.creature.powers.at(game.power_objects, i);
                powers.append(py::make_tuple(power_names[static_cast<int>(power.id)], power.amount));
            }
            monsters.append(py::dict("hp"_a=monster.creature.hp, "block"_a=monster.creature.block,
                "alive"_a=!(monster.creature.dead || monster.creature.escaped || monster.creature.half_dead),
                "damage"_a=monster.intent_damage, "hits"_a=monster.intent_hits,
                "powers"_a=powers));
        }
        const bool keep_selection = game.selection_for_upgrade || (game.manager.has_current_action &&
            (game.action_objects[game.manager.current_action].kind == EffectKind::RetainCards ||
             game.action_objects[game.manager.current_action].kind == EffectKind::NightmareAction ||
             game.action_objects[game.manager.current_action].kind == EffectKind::Setup));
        return py::dict("act"_a=game.run.act, "floor"_a=game.run.floor,
            "hp"_a=game.player.creature.hp, "max_hp"_a=game.player.creature.max_hp,
            "block"_a=game.player.creature.block, "energy"_a=game.player.energy,
            "turn"_a=game.turn, "combat"_a=game.room_in_combat && !game.battle_over,
            "dead"_a=game.player.creature.dead, "victory"_a=game.screen == Screen::Victory,
            "keys"_a=py::make_tuple(game.run.ruby_key, game.run.emerald_key, game.run.sapphire_key),
            "hand"_a=hand, "monsters"_a=monsters, "powers"_a=powers,
            "card_slots"_a=game.cards.count(),
            "choices"_a=choices, "remove"_a=(game.screen == Screen::Discard && !keep_selection) || game.grid_for_purge);
    }
    void step(const py::dict& action) {
        if (game.fault != 0) return;
        spire::step(game, parse_action(action));
    }
};

struct EnvironmentBatch {
    std::vector<Environment> games;
    explicit EnvironmentBatch(std::size_t size) : games(size) {}
};
}

PYBIND11_MODULE(spire, module) {
    using namespace spire;
    using namespace spire::host;
    py::class_<Environment>(module, "Game")
        .def(py::init<>())
        .def("reset", &Environment::reset, "seed"_a, "playtime_at_least_800"_a=false)
        .def("start_combat", &Environment::start_combat, "seed"_a, "scenario"_a)
        .def("legal_actions", &Environment::legal_actions)
        .def("step", &Environment::step, "action"_a)
        .def("snapshot", &Environment::snapshot)
        .def("copy", [](const Environment& env) { return env; })
        .def("search_state", &Environment::search_state)
        .def("schema_gaps", &Environment::schema_gaps)
        .def_property_readonly("fault", [](const Environment& env) { return env.game.fault; });
    module.def("reset", [](std::int64_t seed, bool playtime_at_least_800) { Environment result; result.reset(seed, playtime_at_least_800); return result; }, "seed"_a, "playtime_at_least_800"_a=false);
    module.def("start_combat", [](std::int64_t seed, const py::dict& scenario) {
        Environment result; result.start_combat(seed, scenario); return result;
    }, "seed"_a, "scenario"_a);
    module.def("legal_actions", &Environment::legal_actions, "game"_a);
    module.def("step", &Environment::step, "game"_a, "action"_a);
    module.def("snapshot", &Environment::snapshot, "game"_a);
    py::class_<EnvironmentBatch>(module, "Batch")
        .def(py::init<std::size_t>(), "size"_a)
        .def("reset", [](EnvironmentBatch& batch, const std::vector<std::int64_t>& seeds) {
            SPIRE_ASSERT(seeds.size() == batch.games.size());
            for (std::size_t i = 0; i < batch.games.size(); ++i) batch.games[i].reset(seeds[i]);
        }, "seeds"_a)
        .def("start_combat", [](EnvironmentBatch& batch, const std::vector<std::int64_t>& seeds, const py::dict& scenario) {
            SPIRE_ASSERT(seeds.size() == batch.games.size());
            for (std::size_t i = 0; i < batch.games.size(); ++i) batch.games[i].start_combat(seeds[i], scenario);
        }, "seeds"_a, "scenario"_a)
        .def("legal_actions", [](const EnvironmentBatch& batch) {
            py::list result;
            for (const auto& game : batch.games) result.append(game.legal_actions());
            return result;
        })
        .def("step", [](EnvironmentBatch& batch, const py::list& values) {
            SPIRE_ASSERT(values.size() == batch.games.size());
            for (std::size_t i = 0; i < batch.games.size(); ++i) batch.games[i].step(values[i].cast<py::dict>());
        }, "actions"_a)
        .def("faults", [](const EnvironmentBatch& batch) {
            std::vector<uint32_t> result;
            for (const auto& env : batch.games) result.push_back(env.game.fault);
            return result;
        })
        .def("reset_at", [](EnvironmentBatch& batch, std::size_t index, std::int64_t seed) {
            batch.games[index].reset(seed);
        }, "index"_a, "seed"_a)
        .def("snapshot", [](const EnvironmentBatch& batch) {
            py::list result;
            for (const auto& game : batch.games) result.append(game.snapshot());
            return result;
        })
        .def("__len__", [](const EnvironmentBatch& batch) { return batch.games.size(); });
    module.attr("encounters") = py::cast(std::vector<std::string>(std::begin(encounter_names), std::end(encounter_names)));
}
