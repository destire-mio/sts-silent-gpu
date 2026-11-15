#pragma once
#include "core/engine.hpp"

namespace spire::deck_eval {
struct Features {
    int act, hp, max_hp, boss;
    bool second_boss_available;
    int deck_size, relic_count, potion_count;
    uint16_t cards[master_deck_capacity], upgrades[master_deck_capacity];
    bool removable[master_deck_capacity];
    uint16_t relics[64], potions[potion_capacity];
};

struct Edit {
    int remove[2];
    int add[2];
};

SPIRE_FN inline Features describe(const Game& game) {
    const auto& run = game.run;
    Features result{};
    result.act = run.act;
    result.hp = game.player.creature.hp;
    result.max_hp = game.player.creature.max_hp;
    result.second_boss_available = run.act == 3 && game.room_kind == RoomKind::Boss
        && game.screen == Screen::Room && run.boss_list.size == 2;
    result.boss = 1 + static_cast<int>(result.second_boss_available ? run.boss_list[0] : run.boss_key);
    result.deck_size = run.master_deck.size;
    for (int i = 0; i < result.deck_size; ++i) {
        const Card& card = run.master_deck[i];
        result.cards[i] = 1 + static_cast<int>(card.id);
        result.upgrades[i] = card.upgrades;
        result.removable[i] = !card.bottle_flame && !card.bottle_lightning && !card.bottle_tornado;
    }
    result.relic_count = run.relics.size;
    for (int i = 0; i < result.relic_count; ++i)
        result.relics[i] = 1 + static_cast<int>(run.relics[i].id);
    for (int i = 0; i < run.potion_slots; ++i)
        if (run.potions[i] != PotionId::Empty)
            result.potions[result.potion_count++] = 1 + static_cast<int>(run.potions[i]);
    return result;
}

SPIRE_FN inline void apply(Game& game, const Edit& edit) {
    SPIRE_ASSERT(game.room_phase != RoomPhase::Combat);
    auto& deck = game.run.master_deck;
    for (int index : edit.remove)
        if (index >= 0) deck.erase(index);
    for (int id : edit.add) {
        if (id == 0) continue;
        Card card = make_card(static_cast<CardId>(id - 1));
        card.uuid = game.next_card_uuid++;
        deck.push_back(card);
    }
}
}

#if !defined(__CUDA_ARCH__)
#include "core/card_pool.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <array>
#include <vector>

namespace spire::deck_eval {
namespace py = pybind11;
using namespace pybind11::literals;

inline py::dict describe_python(const Features& row) {
    py::list cards, removable;
    for (int i = 0; i < row.deck_size; ++i) {
        cards.append(py::make_tuple(row.cards[i], row.upgrades[i]));
        if (row.removable[i]) removable.append(i);
    }
    return py::dict("cards"_a=cards, "removable"_a=removable, "hp"_a=row.hp, "max_hp"_a=row.max_hp,
        "act"_a=row.act, "boss"_a=row.boss, "second_boss_available"_a=row.second_boss_available,
        "relics"_a=std::vector<int>(row.relics, row.relics + row.relic_count),
        "potions"_a=std::vector<int>(row.potions, row.potions + row.potion_count));
}

inline Edit decode(const std::array<int, 4>& row) {
    SPIRE_ASSERT(row[0] >= -1 && row[1] >= -1 && (row[1] < 0 || row[0] > row[1]));
    for (int i = 2; i < 4; ++i) SPIRE_ASSERT(row[i] >= 0 && row[i] <= static_cast<int>(CardId::Count));
    return {{row[0], row[1]}, {row[2], row[3]}};
}

inline void register_features(py::module_& module) {
    module.def("silent_reward_card_ids", [] {
        std::vector<int> ids;
        for (const auto& card : common_card_pool) ids.push_back(1 + static_cast<int>(card.native));
        for (const auto& card : uncommon_card_pool) ids.push_back(1 + static_cast<int>(card.native));
        for (const auto& card : rare_card_pool) ids.push_back(1 + static_cast<int>(card.native));
        return ids;
    });
    module.def("max_deck_cards", [] { return master_deck_capacity; });
    module.def("deck_vocab_sizes", [] {
        return std::array<int, 4>{static_cast<int>(CardId::Count) + 1, static_cast<int>(RelicId::Count) + 1,
            static_cast<int>(PotionId::Count) + 1, static_cast<int>(EncounterId::Count) + 1};
    });
}
}
#endif
