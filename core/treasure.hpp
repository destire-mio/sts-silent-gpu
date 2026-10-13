#pragma once
#include "rewards.hpp"

namespace spire {
struct ChestData { int common, uncommon, rare, gold_chance, gold_amount; };
constexpr ChestData chest_data[] = {{75, 25, 0, 50, 25}, {35, 50, 15, 35, 50}, {0, 75, 25, 50, 75}};

SPIRE_FN inline void enter_boss_chest(Game& g) {
    g.chest = ChestState{};
    for (int i = 0; i < 3; ++i) {
        RoomReward reward{};
        reward.kind = RewardKind::Relic;
        reward.relic.id = relic_id(relic_key_name(random_relic_key(g, static_cast<int>(RelicTier::Boss))));
        g.chest.relics.push_back(create_reward(g, reward));
    }
    g.room_phase = RoomPhase::Complete;
    g.screen = Screen::Room;
}

SPIRE_FN inline void open_boss_chest(Game& g) {
    g.chest.open = true;
    for (int i = 0; i < g.run.relics.size; ++i)
        if (!relic_data(g.run.relics[i].id).boss_chest_excluded) relic_onChestOpen(g, i, true);
    g.screen = Screen::BossReward;
}

SPIRE_FN inline void choose_boss_relic(Game& g, int choice) {
    RoomReward& reward = g.reward_objects[g.chest.relics[choice]];
    const int slot = relic_data(reward.relic.id).replaces_starter ? 0 : g.run.relics.size;
    if (slot == g.run.relics.size) g.run.relics.push_back(reward.relic);
    else g.run.relics[slot] = reward.relic;
    reward.relic_slot = slot;
    g.chest.chose_relic = true;
    g.screen = Screen::Room;
    relic_onEquip(g, slot);
}

SPIRE_FN inline void enter_treasure(Game& g) {
    g.chest = ChestState{};
    const int size = g.rng.treasure.random(0, 99);
    g.chest.size = g.run.act == 4 ? 1 : size < 50 ? 0 : size < 83 ? 1 : 2;
    const auto& data = chest_data[g.chest.size];
    const int roll = g.rng.treasure.random(0, 99);
    g.chest.gold = roll < data.gold_chance;
    g.chest.relic = roll < data.common ? RelicTier::Common : roll < data.common + data.uncommon ? RelicTier::Uncommon : RelicTier::Rare;
    g.room_phase = RoomPhase::Complete;
    g.screen = Screen::Room;
}

SPIRE_FN inline void open_chest(Game& g) {
    g.chest.open = true;
    for (int i = 0; i < g.run.relics.size; ++i) relic_onChestOpen(g, i, false);
    const auto& data = chest_data[g.chest.size];
    if (g.chest.gold) add_gold_reward(g, floor_int(g.rng.treasure.random(data.gold_amount * 0.9f, data.gold_amount * 1.1f) + 0.5f));
    if (g.chest.cursed) {
        queue_card_obtain(g, make_card(random_curse(g)));
    }
    add_relic_reward(g, g.chest.relic);
    if (!g.run.sapphire_key) add_key_reward(g, RewardKind::SapphireKey);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onChestOpenAfter(g, i, false);
    open_room_card_rewards(g);
    g.reward_proceed_visible = true;
    g.reward_skippable = false; g.combat_reward_skippable = false;
}
}
