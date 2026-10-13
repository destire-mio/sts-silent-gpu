#pragma once
#include "cards.hpp"
#include "card_pool.hpp"

namespace spire {
SPIRE_FN inline void gain_gold(Game& g, int amount) {
    for (int i = 0; i < g.run.relics.size; ++i)
        if (relic_data(g.run.relics[i].id).blocks_gold) return;
    if (amount <= 0) return;
    g.run.gold += amount;
    for (int i = 0; i < g.run.relics.size; ++i) relic_onGainGold(g, i);
}
SPIRE_FN inline void lose_gold(Game& g, int amount) {
    if (amount <= 0) return;
    g.run.gold = max(0, g.run.gold - amount);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onLoseGold(g, i);
}
SPIRE_FN inline void spend_gold(Game& g, int amount) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_onSpendGold(g, i);
    lose_gold(g, amount);
}
SPIRE_FN inline void equip_relic(Game& g, RelicId id) {
    const int index = g.run.relics.size;
    g.run.relics.push_back({id, relic_data(id).counter});
    relic_initialize_state(g.run.relics[index]);
    relic_onEquip(g, index);
}
SPIRE_FN inline bool unequip_relic(Game& g, RelicId id) {
    int remove_index = -1;
    for (int i = 0; i < g.run.relics.size; ++i) {
        if (g.run.relics[i].id == id) {
            relic_onUnequip(g, i);
            remove_index = i;
        }
    }
    if (remove_index < 0) return false;
    g.run.relics.erase(remove_index);
    return true;
}
SPIRE_FN inline void preview_card_obtain(Game& g, Card& card) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_onPreviewObtainCard(g, i, card);
}
SPIRE_FN inline void obtain_card(Game& g, Card card) {
    const CardRarity rarity = card_rarity(card.id);
    if (rarity != CardRarity::Basic && rarity != CardRarity::Special && rarity != CardRarity::Curse)
        ++g.obtained_cards[static_cast<int>(card.id)];
    if (card_data(card.id).type == CardType::Curse)
        for (int i = 0; i < g.run.relics.size; ++i) if (relic_onCurseObtain(g, i)) return;
    for (int i = 0; i < g.run.relics.size; ++i) relic_onObtainCard(g, i, card);
    g.run.master_deck.push_back(card);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onMasterDeckChange(g, i);
}
SPIRE_FN inline void increase_max_hp(Game& g, int amount) {
    g.player.creature.max_hp += amount;
    heal_creature(g, player_target, amount);
}
SPIRE_FN inline void queue_card_obtain(Game& g, Card card) {
    g.pending_obtains.push_back(create_card(g, card));
}
SPIRE_FN inline void settle_card_obtains(Game& g) {
    for (int i = 0; i < g.pending_obtains.size; ++i) obtain_card(g, g.cards[g.pending_obtains[i]]);
    g.pending_obtains.clear();
}
SPIRE_FN inline void open_master_selection(Game& g, int amount, bool for_purge) {
    g.screen = Screen::Grid;
    g.grid_source = PileId::MasterDeck;
    g.selection_amount = amount;
    g.selection_any_number = false;
    g.grid_for_purge = for_purge;
    g.grid_can_cancel = false;
    g.selected_cards.clear();
}
SPIRE_FN inline void queue_relic_above(Game& g, int relic, int owner, bool top) {
    Effect e{};
    e.kind = EffectKind::RelicAboveCreature;
    action_initialize_state(e);
    e.source = e.target = owner;
    e.extra = relic;
    const int index = g.action_objects.add(e);
    if (top) g.manager.actions.push_front(index);
    else g.manager.actions.push_back(index);
}
SPIRE_FN inline void reward_card_choices(Game& g, List<const char*, reward_card_capacity>& choices) {
    bool any_color = false;
    for (int i = 0; i < g.run.relics.size; ++i) any_color |= relic_data(g.run.relics[i].id).any_color_rewards;
    int count = 3;
    for (int i = 0; i < g.run.relics.size; ++i) count = relic_changeNumberOfCardsInReward(g, i, count);
    SPIRE_ASSERT(count <= reward_card_capacity);
    const CardPoolEntry* pools[] = {common_card_pool, uncommon_card_pool, rare_card_pool};
    const int sizes[] = {static_cast<int>(sizeof(common_card_pool) / sizeof(*common_card_pool)),
        static_cast<int>(sizeof(uncommon_card_pool) / sizeof(*uncommon_card_pool)),
        static_cast<int>(sizeof(rare_card_pool) / sizeof(*rare_card_pool))};
    for (int i = 0; i < count; ++i) {
        const int roll = g.rng.card.random(99) + g.run.card_blizz_randomizer;
        g.room_rare_card_chance = 3;
        g.room_uncommon_card_chance = 37;
        for (int r = 0; r < g.run.relics.size; ++r) {
            g.room_rare_card_chance = relic_changeRareCardRewardChance(g, r, g.room_rare_card_chance);
            g.room_uncommon_card_chance = relic_changeUncommonCardRewardChance(g, r, g.room_uncommon_card_chance);
        }
        const int rarity = roll < g.room_rare_card_chance ? 2 : roll < g.room_rare_card_chance + g.room_uncommon_card_chance ? 1 : 0;
        if (rarity == 2) g.run.card_blizz_randomizer = 5;
        else if (rarity == 0) g.run.card_blizz_randomizer = max(-40, g.run.card_blizz_randomizer - 1);
        const char* id;
        bool duplicate;
        do {
            if (any_color) {
                List<uint16_t, 512> candidates{};
                const CardRarity rarities[]{CardRarity::Common, CardRarity::Uncommon, CardRarity::Rare};
                for (int c = 0; c < static_cast<int>(sizeof(library_reward_cards) / sizeof(*library_reward_cards)); ++c)
                    if (library_reward_cards[c].rarity == rarities[rarity]) candidates.push_back(c);
                g.rng.card.randomLong();
                id = library_reward_cards[candidates[g.rng.card.random(candidates.size - 1)]].id;
            } else id = pools[rarity][g.rng.card.random(sizes[rarity] - 1)].id;
            duplicate = false;
            for (int c = 0; c < choices.size; ++c) if (same_id(choices[c], id)) duplicate = true;
        } while (duplicate);
        choices.push_back(id);
    }
 }
SPIRE_FN inline void add_room_card_reward(Game& g) {
    RoomReward reward{};
    reward.identity = g.next_reward_identity++;
    List<const char*, reward_card_capacity> choices{};
    reward_card_choices(g, choices);
    for (int i = 0; i < choices.size; ++i) reward.cards.push_back(create_card(g, make_card(card_id(choices[i]))));
    for (int i = 0; i < reward.cards.size; ++i) {
        Card& card = g.cards[reward.cards[i]];
        if (card_rarity(card.id) != CardRarity::Rare && g.rng.card.randomBoolean(g.run.card_upgraded_chance) &&
            card_canUpgrade(g, reward.cards[i])) card_upgrade(card);
        else preview_card_obtain(g, card);
    }
    if (!reward.cards.empty()) g.room_rewards.push_back(reward);
}
SPIRE_FN inline void open_room_card_rewards(Game& g) {
    add_room_card_reward(g);
    g.screen = Screen::CombatReward;
}
SPIRE_FN inline void remove_room_relic_reward(Game& g) {
    for (int i = 0; i < g.room_rewards.size; ++i) {
        const RoomReward& reward = g.room_rewards[i];
        if (reward.relic == RelicId::Count) continue;
        const int link = reward.relic_link;
        g.room_rewards.erase(i);
        if (i < g.room_rewards.size && link == g.room_rewards[i].identity) g.room_rewards.erase(i);
        break;
    }
}
SPIRE_FN inline void remove_master_card(Game& g, int index) {
    Card removed = g.run.master_deck.erase(index);
    bool referenced = false;
    for (int i = 0; i < g.run.relics.size; ++i) referenced |= relic_referencesMasterCard(g, i, removed.uuid);
    if (referenced) {
        g.detached_master_cards.push_back(g.cards.size);
        g.cards.push_back(removed);
    }
    card_onRemoveFromMasterDeck(g, removed);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onMasterDeckChange(g, i);
}
SPIRE_FN inline void on_victory(Game& g) {
    for (int i = 0; i < g.run.relics.size; ++i)
        if (relic_data(g.run.relics[i].id).triggers_at_battle_end) relic_onTrigger(g, i);
    if (!g.player.creature.dying) {
        for (int i = 0; i < g.run.relics.size; ++i) relic_onVictory(g, i);
        Powers& powers = g.player.creature.powers;
        for (int i = 0; i < powers.size; ++i) power_onVictory(g, powers.at(g.power_objects, i), player_target);
    }
}
SPIRE_FN inline void gain_energy(Game& g, int amount) {
    g.player.energy = min(999, g.player.energy + amount);
}
SPIRE_FN inline const Card& master_card_by_uuid(const Game& g, uint32_t uuid) {
    for (int i = 0; i < g.run.master_deck.size; ++i)
        if (g.run.master_deck[i].uuid == uuid) return g.run.master_deck[i];
    for (int i = 0; i < g.detached_master_cards.size; ++i)
        if (g.cards[g.detached_master_cards[i]].uuid == uuid) return g.cards[g.detached_master_cards[i]];
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline void master_selection_cards(Game& g, int type) {
    g.grid_cards.clear();
    for (int i = 0; i < g.run.master_deck.size; ++i) {
        const CardId id = g.run.master_deck[i].id;
        if (!card_def(id).purgeable) continue;
        if (type < 0) g.grid_cards.push_back(i);
        else if (static_cast<int>(card_def(id).type) == type) g.grid_cards.insert(0, i);
    }
}
SPIRE_FN inline void master_selection_cards(Game& g, CardType type) { master_selection_cards(g, static_cast<int>(type)); }
SPIRE_FN inline Card transform_card(Game& g, const Card& original, bool upgrade, Random& rng) {
    const CardPoolEntry* pools[] = {common_card_pool, uncommon_card_pool, rare_card_pool, colorless_card_pool, curse_card_pool};
    const int sizes[] = {static_cast<int>(sizeof(common_card_pool) / sizeof(*common_card_pool)),
                         static_cast<int>(sizeof(uncommon_card_pool) / sizeof(*uncommon_card_pool)),
                         static_cast<int>(sizeof(rare_card_pool) / sizeof(*rare_card_pool)),
                         static_cast<int>(sizeof(colorless_card_pool) / sizeof(*colorless_card_pool)),
                         static_cast<int>(sizeof(curse_card_pool) / sizeof(*curse_card_pool))};
    List<CardId, 128> candidates{};
    const CardRarity rarity = card_rarity(original.id);
    const bool colorless = card_colorless(original.id);
    if (rarity == CardRarity::Curse) {
        for (int i = 0; i < sizes[4]; ++i) {
            const CardId id = card_id(pools[4][i].id);
            if (id != original.id) candidates.push_back(id);
        }
    } else if (colorless) {
        for (int i = 0; i < g.run.source_card_pools[3].size; ++i) {
            const CardId id = card_id(pools[3][g.run.source_card_pools[3][i]].id);
            if (id != original.id) candidates.push_back(id);
        }
    } else {
        for (int i = 0; i < sizes[0]; ++i) {
            const CardId id = card_id(pools[0][i].id);
            if (id != original.id) candidates.push_back(id);
        }
        for (int p = 1; p < 3; ++p) for (int i = 0; i < g.run.source_card_pools[p].size; ++i) {
            const CardId id = card_id(pools[p][g.run.source_card_pools[p][i]].id);
            if (id != original.id) candidates.push_back(id);
        }
    }
    Card result = make_card(candidates[rng.random(candidates.size - 1)]);
    if (upgrade) card_upgrade(result);
    result.uuid = g.next_card_uuid++;
    return result;
}
SPIRE_FN inline RelicId relic_id(const char* name) {
    for (int i = 0; i < static_cast<int>(RelicId::Count); ++i)
        if (relic_matches_id(static_cast<RelicId>(i), name)) return static_cast<RelicId>(i);
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline RelicKey random_relic_key(Game& g, int tier, bool from_end) {
    while (true) {
        int pool = tier;
        while (g.run.relic_pools[pool].empty() && (pool < 2 || pool == 3)) pool = pool == 3 ? 1 : pool + 1;
        RelicKey key;
        if (g.run.relic_pools[pool].empty()) {
            // RelicLibrary.getRelic maps AbstractDungeon's exhausted boss-pool key to Circlet.
            key = RelicKey::Circlet;
        }
        else key = g.run.relic_pools[pool].erase(from_end ? g.run.relic_pools[pool].size - 1 : 0);
        if (relic_can_spawn_id(g, relic_id(relic_key_name(key)))) return key;
        from_end = true;
    }
}
SPIRE_FN inline void add_relic_reward(Game& g, RelicTier tier) {
    RoomReward reward{};
    reward.identity = g.next_reward_identity++;
    reward.relic = relic_id(relic_key_name(random_relic_key(g, static_cast<int>(tier))));
    g.room_rewards.push_back(reward);
}
SPIRE_FN inline void settle_relic_acquisition(Game& g, RandomXS128& policy) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_update(g, i);
    if (g.screen == Screen::MasterConfirm) {
        policy.nextInt(1);
        Card card = make_card(g.confirmation_card);
        card.uuid = g.next_card_uuid++;
        obtain_card(g, card);
        g.screen = Screen::None;
        for (int i = 0; i < g.run.relics.size; ++i) relic_update(g, i);
    }
    while (g.screen == Screen::RelicRewards) {
        const int choice = policy.nextInt(g.acquisition_relics.size + 1);
        if (choice == g.acquisition_relics.size) g.screen = Screen::None;
        else {
            const RelicKey key = g.acquisition_relics.erase(choice);
            equip_relic(g, relic_id(relic_key_name(key)));
            for (int i = 0; i < g.run.relics.size; ++i) relic_update(g, i);
        }
    }
    while (g.screen == Screen::PotionRewards) {
        ActionSet actions{};
        for (int i = 0; i < g.run.potion_slots; ++i) {
            if (g.run.potions[i] == PotionId::Empty) continue;
            if (potion_canUse(g, i)) actions.push_back({ActionKind::PotionUse, i, no_target});
            if (potion_canDiscard(g, i)) actions.push_back({ActionKind::PotionDiscard, i, no_target});
        }
        for (int i = 0; i < g.acquisition_potions.size; ++i) actions.push_back({ActionKind::Choose, i, no_target});
        actions.push_back({ActionKind::Proceed, -1, no_target});
        const Action action = actions[policy.nextInt(actions.size)];
        if (action.kind == ActionKind::Proceed) g.screen = Screen::None;
        else if (action.kind == ActionKind::PotionUse || action.kind == ActionKind::PotionDiscard) {
            if (action.kind == ActionKind::PotionUse) {
                potion_use(g, action.index, no_target);
                for (int i = 0; i < g.run.relics.size; ++i) relic_onUsePotion(g, i);
            }
            g.run.potions[action.index] = PotionId::Empty;
            potion_initialize_state(g, action.index);
        } else {
            bool full = true;
            for (int i = 0; i < g.run.potion_slots; ++i) if (g.run.potions[i] == PotionId::Empty) full = false;
            if (!full) {
                const PotionId id = g.acquisition_potions.erase(action.index);
                obtain_potion(g, id, construct_potion(g, id));
            }
        }
    }
    while (g.screen == Screen::Grid || g.screen == Screen::CombatReward || g.screen == Screen::CardReward) {
        ActionSet actions{};
        legal_actions(g, actions);
        SPIRE_ASSERT(!actions.empty());
        step(g, actions[policy.nextInt(actions.size)]);
        for (int i = 0; i < g.run.relics.size; ++i) relic_update(g, i);
    }
    settle_card_obtains(g);
}
}
