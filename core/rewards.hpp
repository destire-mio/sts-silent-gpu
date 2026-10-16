#pragma once
#include "lifecycle.hpp"

namespace spire {
SPIRE_FN inline int create_colorless_reward(Game& g) {
    RoomReward reward{};
    int count = 3;
    for (int i = 0; i < g.run.relics.size; ++i) count = relic_changeNumberOfCardsInReward(g, i, count);
    for (int i = 0; i < count; ++i) {
        const CardRarity rarity = g.rng.card.randomBoolean(0.3f) ? CardRarity::Rare : CardRarity::Uncommon;
        CardId id = random_colorless_card(g, rarity, true);
        if (rarity == CardRarity::Rare) g.run.card_blizz_randomizer = 5;
        bool duplicate;
        do {
            duplicate = false;
            for (int j = 0; j < reward.cards.size; ++j) duplicate |= g.cards[reward.cards[j]].id == id;
            if (duplicate) id = random_colorless_card(g, rarity, true);
        } while (duplicate);
        reward.cards.push_back(create_card(g, make_card(id)));
    }
    for (int i = 0; i < reward.cards.size; ++i) preview_card_obtain(g, g.cards[reward.cards[i]]);
    return create_reward(g, reward);
}
SPIRE_FN inline RelicTier random_relic_tier(Game& g) {
    const int roll = g.rng.relic.random(0, 99);
    return g.run.act == 4 ? RelicTier::Uncommon : roll < 50 ? RelicTier::Common : roll < 83 ? RelicTier::Uncommon : RelicTier::Rare;
}
SPIRE_FN inline RelicTier random_elite_relic_tier(Game& g) {
    // MonsterRoomElite.returnRandomRelicTier does not use TheEnding's dungeon odds.
    const int roll = g.rng.relic.random(0, 99);
    return roll < 50 ? RelicTier::Common : roll < 83 ? RelicTier::Uncommon : RelicTier::Rare;
}
SPIRE_FN inline RelicId random_screenless_relic(Game& g, RelicTier tier) {
    RelicId id;
    do { id = relic_id(random_relic_key(g, static_cast<int>(tier))); }
    while (relic_data(id).screenless_excluded);
    return id;
}

SPIRE_FN inline void add_key_reward(Game& g, RewardKind kind) {
    RoomReward reward{};
    reward.kind = kind;
    const int index = create_reward(g, reward);
    if (kind == RewardKind::SapphireKey) {
        const int relic = g.room_rewards[g.room_rewards.size - 1];
        g.reward_objects[relic].relic_link = g.reward_objects[index].identity;
        g.reward_objects[index].relic_link = g.reward_objects[relic].identity;
    }
    g.room_rewards.push_back(index);
}

SPIRE_FN inline void add_noncamp_relic_reward(Game& g, RelicTier tier) {
    RelicId id;
    do {
        id = relic_id(random_relic_key(g, static_cast<int>(tier)));
    } while (relic_data(id).adds_campfire_option);
    RoomReward reward{};
    reward.kind = RewardKind::Relic;
    reward.relic.id = id;
    g.room_rewards.push_back(create_reward(g, reward));
}

SPIRE_FN inline void combat_rewards(Game& g) {
    bool escaped = true;
    for (int i = 0; i < g.monster_order.size; ++i) escaped &= g.monsters[g.monster_order[i]].creature.escaped;
    if (g.room_kind == RoomKind::Boss) add_gold_reward(g, floor_int((100 + g.rng.misc.random(-5, 5)) * 0.75f + 0.5f));
    else if (g.room_kind == RoomKind::Elite) add_gold_reward(g, g.rng.treasure.random(25, 35));
    else if ((g.room_kind == RoomKind::Monster || g.room_kind == RoomKind::Boss) && !escaped) add_gold_reward(g, g.rng.treasure.random(10, 20));
    if (g.room_kind == RoomKind::Boss && g.run.act >= 3) {
        g.screen = Screen::Room;
        return;
    }
    if (g.room_kind == RoomKind::Elite) {
        add_relic_reward(g, random_elite_relic_tier(g));
        for (int i = 0; i < g.run.relics.size; ++i)
            if (relic_data(g.run.relics[i].id).extra_elite_relic) add_noncamp_relic_reward(g, random_elite_relic_tier(g));
        if (!g.run.emerald_key && g.run.map[g.run.node_y][g.run.node_x].emerald) add_key_reward(g, RewardKind::EmeraldKey);
    }
    int chance = g.room_kind == RoomKind::Elite || g.room_kind == RoomKind::Event ||
        ((g.room_kind == RoomKind::Monster || g.room_kind == RoomKind::Boss) && !escaped) ? 40 + g.run.potion_blizz_modifier : 0;
    for (int i = 0; i < g.run.relics.size; ++i) {
        const int override = relic_data(g.run.relics[i].id).potion_reward_chance;
        if (override >= 0) chance = override;
    }
    if (g.room_rewards.size >= 4) chance = 0;
    if (g.rng.potion.random(0, 99) < chance) {
        add_potion_reward(g, random_potion(g, false));
        g.run.potion_blizz_modifier -= 10;
    } else g.run.potion_blizz_modifier += 10;
    if (!g.room_reward_allowed) { g.screen = Screen::None; return; }
    g.room_reward_time = true;
    if (!g.smoked) open_combat_rewards(g);
    else { g.screen_rewards.clear(); g.screen = Screen::CombatReward; }
    g.reward_skippable = false; g.combat_reward_skippable = false;
    g.reward_proceed_visible = true;
}
}
