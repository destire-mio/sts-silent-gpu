#pragma once
#include "rewards.hpp"

namespace spire {
struct ChestData { int common, uncommon, rare, gold_chance, gold_amount; };
constexpr ChestData chest_data[] = {{75, 25, 0, 50, 25}, {35, 50, 15, 35, 50}, {0, 75, 25, 50, 75}};

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
        constexpr const char* curses[] = {"Regret", "Injury", "Shame", "Parasite", "Normality", "Doubt", "Writhe", "Pain", "Decay", "Clumsy"};
        queue_card_obtain(g, make_card(card_id(curses[g.rng.card.random(0, 9)])));
    }
    add_relic_reward(g, g.chest.relic);
    if (!g.run.sapphire_key) add_key_reward(g, RewardKind::SapphireKey);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onChestOpenAfter(g, i, false);
    open_room_card_rewards(g);
    g.reward_proceed_visible = true;
    g.reward_skippable = false; g.combat_reward_skippable = false;
}
}
