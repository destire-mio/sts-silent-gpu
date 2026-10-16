#pragma once
#include "lifecycle.hpp"

namespace spire {
SPIRE_FN inline bool shop_restock(const Game& g) {
    for (int i = 0; i < g.run.relics.size; ++i) if (relic_data(g.run.relics[i].id).restocks_shop) return true;
    return false;
}
SPIRE_FN inline CardId shop_card(Game& g, int type, bool seeded, CardRarity colorless = CardRarity::Basic) {
    if (colorless != CardRarity::Basic) return random_colorless_card(g, colorless, seeded);
    List<CardId, 64> choices{};
    const int roll = g.rng.card.random(99) + g.run.card_blizz_randomizer;
    g.room_rare_card_chance = 9;
    g.room_uncommon_card_chance = 37;
    int rarity = roll < 9 ? 2 : roll < 46 ? 1 : 0;
    if (type == static_cast<int>(CardType::Power) && rarity == 0) rarity = 1;
    const CardPoolEntry* pools[] = {common_card_pool, uncommon_card_pool, rare_card_pool};
    const int sizes[] = {sizeof(common_card_pool) / sizeof(*common_card_pool), sizeof(uncommon_card_pool) / sizeof(*uncommon_card_pool), sizeof(rare_card_pool) / sizeof(*rare_card_pool)};
    for (int i = 0; i < sizes[rarity]; ++i)
        if (static_cast<int>(pools[rarity][i].type) == type) choices.push_back(pools[rarity][i].native);
    for (int i = 1; i < choices.size; ++i) {
        const CardId card = choices[i];
        int j = i;
        while (j > 0 && card_sort_order(choices[j - 1]) > card_sort_order(card)) { choices[j] = choices[j - 1]; --j; }
        choices[j] = card;
    }
    return choices[seeded ? g.rng.card.random(choices.size - 1) : g.unseeded.random(choices.size - 1)];
}
SPIRE_FN inline int shop_card_base_price(CardId card) {
    const CardRarity rarity = card_rarity(card);
    return rarity == CardRarity::Common ? 50 : rarity == CardRarity::Uncommon ? 75 : 150;
}
SPIRE_FN inline int shop_potion_price(PotionId id) {
    for (const auto& entry : silent_potion_pool)
        if (id == entry.id) return entry.rarity == 0 ? 50 : entry.rarity == 1 ? 75 : 100;
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline RelicTier shop_relic_tier(Game& g) {
    const int roll = g.rng.merchant.random(99);
    return roll < 48 ? RelicTier::Common : roll < 82 ? RelicTier::Uncommon : RelicTier::Rare;
}
SPIRE_FN inline void shop_discount(Game& g, float multiplier, bool purge) {
    for (int i = 0; i < g.shop.relics.size; ++i) g.shop.relics[i].price = floor_int(g.shop.relics[i].price * multiplier + 0.5f);
    for (int i = 0; i < g.shop.potions.size; ++i) g.shop.potions[i].price = floor_int(g.shop.potions[i].price * multiplier + 0.5f);
    for (int i = 0; i < g.shop.colored.size; ++i) {
        Card& card = g.cards[g.shop.colored[i]];
        card.price = floor_int(card.price * multiplier + 0.5f);
    }
    for (int i = 0; i < g.shop.colorless.size; ++i) {
        Card& card = g.cards[g.shop.colorless[i]];
        card.price = floor_int(card.price * multiplier + 0.5f);
    }
    if (purge) g.run.actual_purge_cost = floor_int(g.run.purge_cost * multiplier + 0.5f);
    for (int i = 0; i < g.run.relics.size; ++i)
        if (relic_data(g.run.relics[i].id).purge_cost >= 0) g.run.actual_purge_cost = relic_data(g.run.relics[i].id).purge_cost;
}
SPIRE_FN inline int shop_restock_price(Game& g, int price) {
    price = floor_int(price * g.rng.merchant.random(0.95f, 1.05f) + 0.5f);
    if (shop_restock(g)) price = floor_int(price * 0.8f + 0.5f);
    for (int i = 0; i < g.run.relics.size; ++i)
        if (relic_data(g.run.relics[i].id).discounts_existing_stock) price = floor_int(price * relic_data(g.run.relics[i].id).shop_multiplier + 0.5f);
    return price;
}
SPIRE_FN inline void enter_shop(Game& g) {
    g.shop = ShopState{};
    for (int i = 0; i < 5; ++i) {
        const int type = static_cast<int>(i < 2 ? CardType::Attack : i < 4 ? CardType::Skill : CardType::Power);
        CardId card;
        do { card = shop_card(g, type, true); }
        while ((i == 1 || i == 3) && card == g.cards[g.shop.colored[i - 1]].id);
        g.shop.colored.push_back(create_card(g, make_card(card)));
    }
    const CardRarity rarities[] = {CardRarity::Uncommon, CardRarity::Rare};
    for (const auto rarity : rarities)
        g.shop.colorless.push_back(create_card(g, make_card(shop_card(g, -1, true, rarity))));
    for (int i = 0; i < 7; ++i) {
        Card& card = g.cards[i < 5 ? g.shop.colored[i] : g.shop.colorless[i - 5]];
        float price = shop_card_base_price(card.id) * g.rng.merchant.random(0.9f, 1.1f);
        if (i >= 5) price *= 1.2f;
        card.priced = true; card.price = static_cast<int>(price);
        preview_card_obtain(g, card);
    }
    g.cards[g.shop.colored[g.rng.merchant.random(0, 4)]].price /= 2;
    for (int i = 0; i < 3; ++i) {
        const RelicTier tier = i == 2 ? RelicTier::Shop : shop_relic_tier(g);
        RoomReward reward{};
        reward.kind = RewardKind::Relic;
        reward.relic.id = relic_id(random_relic_key(g, static_cast<int>(tier), true));
        const int index = create_reward(g, reward);
        const int price = floor_int(relic_data(reward.relic.id).price * g.rng.merchant.random(0.95f, 1.05f) + 0.5f);
        g.shop.relics.push_back({static_cast<uint8_t>(index), static_cast<uint8_t>(i), static_cast<int16_t>(price)});
    }
    for (int i = 0; i < 3; ++i) {
        RoomReward reward{};
        reward.kind = RewardKind::Potion;
        reward.potion = random_potion(g, false);
        reward.potion_state = construct_potion(g, reward.potion);
        const int index = create_reward(g, reward);
        const int price = floor_int(shop_potion_price(reward.potion) * g.rng.merchant.random(0.95f, 1.05f) + 0.5f);
        g.shop.potions.push_back({static_cast<uint8_t>(index), static_cast<uint8_t>(i), static_cast<int16_t>(price)});
    }
    g.run.actual_purge_cost = g.run.purge_cost;
    shop_discount(g, 1.1f, false);
    if (shop_restock(g)) shop_discount(g, 0.8f, true);
    for (int i = 0; i < g.run.relics.size; ++i)
        if (relic_data(g.run.relics[i].id).discounts_existing_stock) shop_discount(g, relic_data(g.run.relics[i].id).shop_multiplier, true);
    g.screen = Screen::Room;
    g.room_phase = RoomPhase::Complete;
}
SPIRE_FN inline void shop_choices(const Game& g, List<uint8_t, 14>& choices) {
    choices.clear();
    if (g.shop.purge_available && g.run.gold >= g.run.actual_purge_cost) choices.push_back(0);
    for (int i = 0; i < g.shop.colored.size; ++i) if (g.cards[g.shop.colored[i]].price <= g.run.gold) choices.push_back(1 + i);
    for (int i = 0; i < g.shop.colorless.size; ++i) if (g.cards[g.shop.colorless[i]].price <= g.run.gold) choices.push_back(6 + i);
    for (int i = 0; i < g.shop.relics.size; ++i) if (g.shop.relics[i].price <= g.run.gold) choices.push_back(8 + i);
    for (int i = 0; i < g.shop.potions.size; ++i) if (g.shop.potions[i].price <= g.run.gold) choices.push_back(11 + i);
}
SPIRE_FN inline void finish_shop_purge(Game& g) {
    if (g.selected_cards.empty()) return;
    lose_gold(g, g.run.actual_purge_cost);
    g.run.purge_cost += 25;
    float price = static_cast<float>(g.run.purge_cost);
    if (shop_restock(g)) price *= 0.8f;
    for (int i = 0; i < g.run.relics.size; ++i)
        if (relic_data(g.run.relics[i].id).discounts_existing_stock) price *= relic_data(g.run.relics[i].id).shop_multiplier;
    g.run.actual_purge_cost = floor_int(price + 0.5f);
    for (int i = 0; i < g.run.relics.size; ++i)
        if (relic_data(g.run.relics[i].id).purge_cost >= 0) g.run.actual_purge_cost = relic_data(g.run.relics[i].id).purge_cost;
    for (int i = 0; i < g.selected_cards.size; ++i) remove_master_card(g, g.selected_cards[i]);
    g.selected_cards.clear();
    g.shop.purge_available = false;
}
SPIRE_FN inline void purchase_shop(Game& g, int choice) {
    if (choice == 0) {
        master_selection_cards(g, -1);
        for (int i = g.grid_cards.size - 1; i >= 0; --i) {
            const Card& card = g.run.master_deck[g.grid_cards[i]];
            if (card.bottle_flame || card.bottle_lightning || card.bottle_tornado) g.grid_cards.erase(i);
        }
        open_master_selection(g, 1, true);
        g.grid_can_cancel = true;
        g.previous_screen = Screen::Shop;
    } else if (choice < 8) {
        const bool colorless = choice >= 6;
        const int position = choice - (colorless ? 6 : 1);
        const int index = colorless ? g.shop.colorless[position] : g.shop.colored[position];
        const Card card = g.cards[index];
        lose_gold(g, card.price);
        if (shop_restock(g)) {
            const CardId replacement = colorless ? shop_card(g, -1, true, g.rng.merchant.random() < 0.3f ? CardRarity::Rare : CardRarity::Uncommon) : shop_card(g, static_cast<int>(card_data(card.id).type), false);
            Card stock = make_card(replacement);
            preview_card_obtain(g, stock);
            float price = shop_card_base_price(stock.id) * g.rng.merchant.random(0.9f, 1.1f);
            if (colorless) price *= 1.2f;
            price *= 0.8f;
            for (int i = 0; i < g.run.relics.size; ++i)
                if (relic_data(g.run.relics[i].id).discounts_existing_stock) price *= relic_data(g.run.relics[i].id).shop_multiplier;
            stock.priced = true; stock.price = static_cast<int>(price);
            stock.uuid = g.next_card_uuid++;
            g.cards[index] = stock;
        } else if (colorless) g.shop.colorless.erase(position);
        else g.shop.colored.erase(position);
        obtain_card(g, card);
    } else if (choice < 11) {
        const int position = choice - 8;
        auto& item = g.shop.relics[position];
        auto& reward = g.reward_objects[item.reward];
        const RelicId id = reward.relic.id;
        lose_gold(g, item.price);
        const int acquired = g.run.relics.size;
        equip_relic(g, id);
        const auto& data = relic_data(id);
        if (data.discounts_existing_stock) shop_discount(g, data.shop_multiplier, true);
        if (data.purge_cost >= 0) g.run.actual_purge_cost = data.purge_cost;
        if (g.run.relics.size > acquired) {
            for (int i = 0; i < g.shop.colored.size; ++i) relic_onPreviewObtainCard(g, acquired, g.cards[g.shop.colored[i]]);
            for (int i = 0; i < g.shop.colorless.size; ++i) relic_onPreviewObtainCard(g, acquired, g.cards[g.shop.colorless[i]]);
        }
        if (shop_restock(g)) {
            RelicId replacement;
            do { replacement = relic_id(random_relic_key(g, static_cast<int>(shop_relic_tier(g)), true)); }
            while (relic_data(replacement).excluded_from_restock);
            reward.relic = {replacement, relic_data(replacement).counter};
            relic_initialize_state(reward.relic);
            item.price = shop_restock_price(g, relic_data(replacement).price);
        } else g.shop.relics.erase(position);
        settle_card_obtains(g);
    } else {
        const int position = choice - 11;
        auto& item = g.shop.potions[position];
        auto& reward = g.reward_objects[item.reward];
        for (int i = 0; i < g.run.relics.size; ++i) if (relic_data(g.run.relics[i].id).blocks_potions) return;
        int slot = -1;
        for (int i = 0; i < g.run.potion_slots; ++i) if (g.run.potions[i] == PotionId::Empty) { slot = i; break; }
        if (slot < 0) return;
        obtain_potion(g, reward.potion, reward.potion_state);
        lose_gold(g, item.price);
        if (shop_restock(g)) {
            reward.potion = random_potion(g, false);
            reward.potion_state = construct_potion(g, reward.potion);
            item.price = shop_restock_price(g, shop_potion_price(reward.potion));
        } else g.shop.potions.erase(position);
    }
}
}
