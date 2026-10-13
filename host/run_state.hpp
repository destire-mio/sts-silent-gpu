#pragma once
#include "core/engine.hpp"
#include "core/card_pool.hpp"
#include "names.hpp"
#include <string>
#include <vector>

namespace spire::host {
struct FreshRunState {
    std::vector<std::string> relicsToRemoveOnStart;
    std::vector<std::string> rewardTypes, rewardCards;
    std::string rewardPotion;
    int rewardGold = 0;
    int blizzardPotionMod = 0;
};

inline void settle_combat_rewards(Game& game, FreshRunState& run) {
    bool escaped = true;
    for (int i = 0; i < game.monster_order.size; ++i) {
        const Monster& monster = game.monsters[game.monster_order[i]];
        escaped &= monster.creature.escaped;
    }
    if (game.room_stolen_gold > 0) run.rewardTypes.push_back("STOLEN_GOLD");
    if (!escaped) {
        run.rewardGold = game.rng.treasure.random(10, 20);
        run.rewardTypes.push_back("GOLD");
    }
    int chance = escaped ? 0 : 40 + run.blizzardPotionMod;
    for (int i = 0; i < game.run.relics.size; ++i) {
        const int chance_override = relic_data(game.run.relics[i].id).potion_reward_chance;
        if (chance_override >= 0) chance = chance_override;
    }
    if (game.rng.potion.random(0, 99) < chance) {
        const int roll = game.rng.potion.random(0, 99);
        const int rarity = roll < 65 ? 0 : roll < 90 ? 1 : 2;
        int index;
        do { index = game.rng.potion.random(32); } while (silent_potion_pool[index].rarity != rarity);
        run.rewardPotion = silent_potion_pool[index].id;
        run.rewardTypes.push_back("POTION");
        run.blizzardPotionMod -= 10;
    } else run.blizzardPotionMod += 10;
    if (game.smoked) return;
    bool any_color = false;
    for (int i = 0; i < game.run.relics.size; ++i) any_color |= relic_data(game.run.relics[i].id).any_color_rewards;
    if (any_color) {
        List<const char*, reward_card_capacity> choices{};
        reward_card_choices(game, choices);
        for (int i = 0; i < choices.size; ++i) run.rewardCards.push_back(choices[i]);
        for (const auto& id : run.rewardCards) {
            for (const auto& entry : library_reward_cards)
                if (id == entry.id && entry.rarity != CardRarity::Rare) game.rng.card.randomBoolean(game.run.card_upgraded_chance);
        }
        run.rewardTypes.push_back("CARD");
        return;
    }
    add_room_card_reward(game);
    for (int i = 0; i < game.run.relics.size; ++i) {
        if (relic_data(game.run.relics[i].id).extra_normal_combat_card_reward) {
            add_room_card_reward(game);
            break;
        }
    }
    for (int i = 0; i < game.room_rewards.size; ++i) run.rewardTypes.push_back("CARD");
}

inline void initialize_fresh_run(FreshRunState& run) {
    run = FreshRunState{};
    // CharacterManager and CharacterSelectScreen each construct the four characters.
    for (int creation = 0; creation < 2; ++creation) {
        for (const char* relic : {"Burning Blood", "Ring of the Snake", "Cracked Core", "PureWater"}) {
            run.relicsToRemoveOnStart.push_back(relic);
        }
    }
    run.relicsToRemoveOnStart.push_back("Ring of the Snake");
}

}
