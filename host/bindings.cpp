#include "batch.hpp"
#include "snapshot.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace pybind11::literals;

namespace spire::host {
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

    void reset(std::int64_t seed) {
        spire::reset(game, seed);
        fresh = FreshRunState{};
        initialize_fresh_run(fresh);
    }
    void start_combat(std::int64_t seed, const py::dict& scenario) {
        RunProfile profile{};
        if (scenario.contains("profile")) {
            const py::dict value = scenario["profile"].cast<py::dict>();
            profile.neow_spirits = value["neow_spirits"].cast<int>();
            const char* keys[] = {"GUARDIAN", "GHOST", "SLIME", "CHAMP", "AUTOMATON", "COLLECTOR", "CROW", "DONUT", "WIZARD"};
            for (auto item : value["bosses_seen"].cast<py::list>()) {
                const auto key = py::cast<std::string>(item);
                int i = 0; while (i < 9 && key != keys[i]) ++i;
                SPIRE_ASSERT(i < 9); profile.bosses_seen |= 1 << i;
            }
        }
        spire::reset(game, seed, profile);
        fresh = FreshRunState{}; initialize_fresh_run(fresh);
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
        .def("reset", &Environment::reset, "seed"_a)
        .def("start_combat", &Environment::start_combat, "seed"_a, "scenario"_a)
        .def("legal_actions", &Environment::legal_actions)
        .def("step", &Environment::step, "action"_a)
        .def("snapshot", &Environment::snapshot)
        .def("schema_gaps", &Environment::schema_gaps)
        .def_property_readonly("fault", [](const Environment& env) { return env.game.fault; });
    module.def("reset", [](std::int64_t seed) { Environment result; result.reset(seed); return result; }, "seed"_a);
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
