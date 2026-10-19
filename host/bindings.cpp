#include "batch.hpp"
#include "snapshot.hpp"
#include "training_batch.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <algorithm>
#include <type_traits>

#include "tensor_binding.hpp"

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
    obs::History reveal{};

    void reset(std::int64_t seed, bool playtime_at_least_800 = false) {
        RunProfile profile{};
        profile.playtime_at_least_800 = playtime_at_least_800;
        spire::reset(game, seed, profile);
        reveal = {};
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
        reveal = {};
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
        const auto selected = parse_action(action);
        spire::step(game, selected);
    }
};

struct EnvironmentBatch {
    std::vector<Environment> games;
    explicit EnvironmentBatch(std::size_t size) : games(size) {}
};

py::tuple observation(Environment& env) {
    obs::Observation encoded;
    ActionSet actions;
    const auto result = obs::encode(env.game, env.reveal, encoded.view(), actions);
    py::array_t<uint8_t> tags({obs::tag_rows, obs::entity_capacity});
    py::array_t<int16_t> ids({obs::id_rows, obs::entity_capacity}), values({obs::value_rows, obs::entity_capacity});
    py::array_t<uint8_t> kinds(obs::candidate_capacity);
    py::array_t<int16_t> refs({obs::candidate_rows, obs::candidate_capacity});
    py::array_t<uint8_t> entities(obs::entity_mask_bytes), candidates(obs::candidate_mask_bytes);
    std::copy_n(encoded.tags, tags.size(), tags.mutable_data());
    std::copy_n(encoded.ids, ids.size(), ids.mutable_data());
    std::copy_n(encoded.values, values.size(), values.mutable_data());
    std::copy_n(encoded.candidate_kinds, kinds.size(), kinds.mutable_data());
    std::copy_n(encoded.candidate_refs, refs.size(), refs.mutable_data());
    std::copy_n(encoded.entity_mask, entities.size(), entities.mutable_data());
    std::copy_n(encoded.candidate_mask, candidates.size(), candidates.mutable_data());
    return py::make_tuple(tags, ids, values, kinds, refs, entities, candidates, result.fault);
}

template<class T, class Batch>
py::array buffer(Batch& batch, std::vector<T>& data, std::vector<py::ssize_t> shape) {
    py::array result(py::dtype::of<T>(), shape, data.data(), py::cast(&batch, py::return_value_policy::reference));
    result.attr("setflags")(false);
    return result;
}
template<class Batch>
void observation_arrays(py::class_<Batch>& cls) {
    cls.def_property_readonly("tags", [](Batch& b) { return buffer(b, b.tags, {b.size, obs::tag_rows, obs::entity_capacity}); })
        .def_property_readonly("ids", [](Batch& b) { return buffer(b, b.ids, {b.size, obs::id_rows, obs::entity_capacity}); })
        .def_property_readonly("values", [](Batch& b) { return buffer(b, b.values, {b.size, obs::value_rows, obs::entity_capacity}); })
        .def_property_readonly("candidate_kinds", [](Batch& b) { return buffer(b, b.candidate_kinds, {b.size, obs::candidate_capacity}); })
        .def_property_readonly("candidate_refs", [](Batch& b) { return buffer(b, b.candidate_refs, {b.size, obs::candidate_rows, obs::candidate_capacity}); })
        .def_property_readonly("entity_mask", [](Batch& b) { return buffer(b, b.entity_mask, {b.size, obs::entity_mask_bytes}); })
        .def_property_readonly("candidate_mask", [](Batch& b) { return buffer(b, b.candidate_mask, {b.size, obs::candidate_mask_bytes}); })
        .def_property_readonly("entity_counts", [](Batch& b) { return buffer(b, b.entity_counts, {b.size}); })
        .def_property_readonly("candidate_counts", [](Batch& b) { return buffer(b, b.candidate_counts, {b.size}); });
}

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
        .def("observation", &observation)
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
    auto training = py::class_<TrainingBatch>(module, "TrainingBatch");
    root_cache_methods<spire::host::RootCache>(module, training,
        [](TrainingBatch& b, spire::host::RootCache& cache, py::object slots, py::object roots) {
            const int count = slots.attr("numel")().cast<int>();
            auto* s = tensor_pointer<int64_t>(slots, "int64", -1);
            auto* r = tensor_pointer<int64_t>(roots, "int64", -1);
            py::gil_scoped_release release;
            b.cache_roots(cache, s, r, count);
        },
        [](TrainingBatch& b, const spire::host::RootCache& cache, py::object slots, py::object roots) {
            const int count = slots.attr("numel")().cast<int>();
            auto* s = tensor_pointer<int64_t>(slots, "int64", -1);
            auto* r = tensor_pointer<int64_t>(roots, "int64", -1);
            py::gil_scoped_release release;
            b.reset_cached(cache, s, r, count);
        });
    tensor_methods(training,
        [](TrainingBatch& b, py::object choices, py::object active) {
            auto* c = tensor_pointer<int32_t>(choices, "int32", -1);
            auto* a = tensor_pointer<uint8_t>(active, "uint8", -1);
            py::gil_scoped_release release;
            b.step_tensors(c, a);
        },
        [](TrainingBatch& b, py::object slots, py::object roots, py::object seeds,
           py::object offsets, py::object candidates) {
            const int count = slots.attr("numel")().cast<int>();
            auto* s = tensor_pointer<int64_t>(slots, "int64", -1);
            auto* r = tensor_pointer<int64_t>(roots, "int64", -1);
            auto* e = tensor_pointer<int64_t>(seeds, "int64", -1);
            auto* o = tensor_pointer<int64_t>(offsets, "int64", -1);
            auto* c = tensor_pointer<int32_t>(candidates, "int32", -1);
            py::gil_scoped_release release;
            b.reset_tensors(s, r, count, e, o, c);
        },
        [](TrainingBatch& b, py::object slots, py::object data, py::object offsets) {
            const int count = slots.attr("numel")().cast<int>();
            auto* s = tensor_pointer<int64_t>(slots, "int64", -1);
            auto* d = tensor_pointer<uint8_t>(data, "uint8", -1);
            auto* o = tensor_pointer<int64_t>(offsets, "int64", -1);
            py::gil_scoped_release release;
            b.store_tensors(s, count, d, o);
        });
    observation_arrays(training);
    training
        .def(py::init([](int size, int threads, uint64_t cap, const std::vector<int64_t>& seeds) {
            py::gil_scoped_release release;
            return std::make_unique<TrainingBatch>(size, threads, cap, seeds);
        }), "size"_a, "threads"_a, "decision_cap"_a, "seeds"_a)
        .def("step", [](TrainingBatch& batch, py::array_t<int32_t, py::array::c_style> selected) {
            if (selected.ndim() != 1 || selected.size() != batch.size) throw py::value_error("Expected one int32 candidate index per game");
            py::gil_scoped_release release;
            batch.step(selected.data());
        }, "candidate_indices"_a.noconvert())
        .def("step_slots", &TrainingBatch::step_slots, "slots"_a, "candidate_indices"_a, py::call_guard<py::gil_scoped_release>())
        .def("reset_to", &TrainingBatch::reset_to, "slot"_a, "seed"_a, "candidates"_a, py::call_guard<py::gil_scoped_release>())
        .def("reset_to_many", &TrainingBatch::reset_to_many, "slots"_a, "seeds"_a, "candidates"_a, py::call_guard<py::gil_scoped_release>())
        .def("clone", &TrainingBatch::clone, "source"_a, "target"_a, py::call_guard<py::gil_scoped_release>())
        .def("extend_seeds", &TrainingBatch::extend_seeds, "seeds"_a, py::call_guard<py::gil_scoped_release>())
        .def_property_readonly("rewards", [](TrainingBatch& b) { return buffer(b, b.rewards, {b.size}); })
        .def_property_readonly("done", [](TrainingBatch& b) { return buffer(b, b.done, {b.size}); })
        .def_property_readonly("truncated", [](TrainingBatch& b) { return buffer(b, b.truncated, {b.size}); })
        .def_property_readonly("faults", [](TrainingBatch& b) { return buffer(b, b.faults, {b.size}); })
        .def_property_readonly("reset_mask", [](TrainingBatch& b) { return buffer(b, b.reset_mask, {b.size}); })
        .def_property_readonly("advanced", [](TrainingBatch& b) { return buffer(b, b.advanced, {b.size}); })
        .def_property_readonly("seed_indices", [](TrainingBatch& b) { return buffer(b, b.seed_indices, {b.size}); })
        .def_property_readonly("decision_counts", [](TrainingBatch& b) { return buffer(b, b.decision_counts, {b.size}); })
        .def_property_readonly("timing_ns", [](TrainingBatch& b) { return buffer(b, b.timing_ns, {b.size, 3}); });
    training.def("store", [](TrainingBatch& batch, py::array_t<int32_t, py::array::c_style> slots,
            py::array_t<uint8_t, py::array::c_style> data, py::array_t<uint64_t, py::array::c_style> offsets) {
        if (slots.ndim() != 1 || data.ndim() != 1 || offsets.ndim() != 1 || offsets.size() != slots.size() + 1)
            throw std::invalid_argument("store expects slots[M], data[bytes], offsets[M+1]");
        for (py::ssize_t i = 0; i < slots.size(); ++i) if (slots.data()[i] < 0 || slots.data()[i] >= batch.size)
            throw std::invalid_argument("store slot outside batch");
        auto* output = data.mutable_data(); auto* positions = offsets.mutable_data();
        py::gil_scoped_release release;
        return batch.store(slots.data(), slots.size(), output, data.size(), positions);
    }, "slots"_a.noconvert(), "data"_a.noconvert(), "offsets"_a.noconvert());
    auto padded = py::class_<ObservationBatch>(module, "ObservationBatch");
    observation_arrays(padded);
    padded.def(py::init<int>(), "size"_a)
        .def("pad", [](ObservationBatch& batch, py::array_t<uint8_t, py::array::c_style> data,
                py::array_t<uint64_t, py::array::c_style> offsets, py::array_t<int32_t, py::array::c_style> indices) {
            if (data.ndim() != 1 || offsets.ndim() != 1 || indices.ndim() != 1 || indices.size() != batch.size)
                throw std::invalid_argument("pad expects data[bytes], offsets[records+1], indices[batch size]");
            for (py::ssize_t i = 0; i < indices.size(); ++i) if (indices.data()[i] < 0 || indices.data()[i] >= offsets.size() - 1)
                throw std::invalid_argument("pad record outside offsets");
            py::gil_scoped_release release;
            batch.pad(data.data(), offsets.data(), indices.data());
        }, "data"_a.noconvert(), "offsets"_a.noconvert(), "indices"_a.noconvert());
    module.attr("entity_bytes") = obs::entity_bytes;
    module.attr("candidate_bytes") = obs::candidate_bytes;
    module.attr("observation_bytes") = sizeof(obs::Observation);
    module.attr("game_bytes") = sizeof(Game);
}
