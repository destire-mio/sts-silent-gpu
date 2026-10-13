#pragma once
#include "rewards.hpp"

namespace spire {
SPIRE_FN inline RoomKind roll_event_room(Game& g) {
    Random rng(g.run.seed, g.rng.event.counter);
    const int roll = static_cast<int>(rng.random() * 100.0f);
    bool chest = false;
    for (int i = 0; i < g.run.relics.size; ++i) {
        const int interval = relic_data(g.run.relics[i].id).event_chest_interval;
        if (interval && ++g.run.relics[i].counter == interval) { g.run.relics[i].counter = 0; chest = true; }
    }
    auto& chance = g.run.event_chances;
    const int monster = static_cast<int>(chance[1] * 100.0f);
    const int shop = g.room_kind == RoomKind::Shop ? 0 : static_cast<int>(chance[2] * 100.0f);
    const int treasure = static_cast<int>(chance[3] * 100.0f);
    RoomKind room = RoomKind::Event;
    if (roll < min(100, monster)) room = RoomKind::Monster;
    if (roll >= min(99, monster) && roll < min(100, monster + shop)) room = RoomKind::Shop;
    if (roll >= min(99, monster + shop) && roll < min(100, monster + shop + treasure)) room = RoomKind::Treasure;
    if (chest) room = RoomKind::Treasure;
    chance[0] += 0.1f;
    if (room == RoomKind::Monster) {
        for (int i = 0; i < g.run.relics.size; ++i)
            if (relic_data(g.run.relics[i].id).prevents_event_combat) room = RoomKind::Event;
        chance[1] = 0.1f;
    } else chance[1] += 0.1f;
    chance[2] = room == RoomKind::Shop ? 0.03f : chance[2] + 0.03f;
    chance[3] = room == RoomKind::Treasure ? 0.02f : chance[3] + 0.02f;
    g.rng.event = rng;
    return room;
}

SPIRE_FN inline void enter_event(Game& g) {
    Random rng(g.run.seed, g.rng.event.counter);
    const bool shrine = rng.random(1.0f) < 0.25f;
    List<EventKey, 32> candidates{};
    if (!shrine || (g.run.shrine_list.empty() && g.run.special_events.empty()))
        for (int i = 0; i < g.run.event_list.size; ++i)
            if (event_canSpawn(g, event_id(g.run.event_list[i]))) candidates.push_back(g.run.event_list[i]);
    if (candidates.empty()) {
        for (int i = 0; i < g.run.shrine_list.size; ++i) candidates.push_back(g.run.shrine_list[i]);
        for (int i = 0; i < g.run.special_events.size; ++i)
            if (event_canSpawn(g, event_id(g.run.special_events[i]))) candidates.push_back(g.run.special_events[i]);
    }
    const auto key = candidates[rng.random(candidates.size - 1)];
    for (int i = 0; i < g.run.event_list.size; ++i) if (g.run.event_list[i] == key) { g.run.event_list.erase(i); break; }
    for (int i = 0; i < g.run.shrine_list.size; ++i) if (g.run.shrine_list[i] == key) { g.run.shrine_list.erase(i); break; }
    for (int i = 0; i < g.run.special_events.size; ++i) if (g.run.special_events[i] == key) { g.run.special_events.erase(i); break; }
    g.event.id = event_id(key);
    event_initialize_state(g.event);
    g.room_dialog.clear(); g.image_dialog.clear();
    g.room_event_class = g.text.intern(event_data(g.event.id).java_class);
    g.room_phase = RoomPhase::Event;
    g.screen = Screen::Event;
    event_enter(g);
}

SPIRE_FN inline void event_dialog_choices(const Game& g, EventChoices& choices) {
    const auto& buttons = event_data(g.event.id).image ? g.image_dialog : g.room_dialog;
    for (int i = 0; i < buttons.size; ++i) if (!buttons[i]) choices.push_back({EventChoiceKind::Option, static_cast<uint8_t>(i)});
}

SPIRE_FN inline void event_options(Game& g, int count) {
    auto& options = event_data(g.event.id).image ? g.image_dialog : g.room_dialog;
    options.clear();
    for (int i = 0; i < count; ++i) options.push_back(false);
}

SPIRE_FN inline void event_purge_cards(Game& g) {
    master_selection_cards(g, -1);
    for (int i = g.grid_cards.size - 1; i >= 0; --i) {
        const Card& card = g.run.master_deck[g.grid_cards[i]];
        if (card.bottle_flame || card.bottle_lightning || card.bottle_tornado) g.grid_cards.erase(i);
    }
}

SPIRE_FN inline void event_damage(Game& g, int amount, int source) {
    DamageInfo damage{};
    damage.source = source; damage.target = player_target; damage.amount = amount;
    player_damage(g, damage);
}
}
