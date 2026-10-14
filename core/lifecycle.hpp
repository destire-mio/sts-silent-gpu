#pragma once
#include "cards.hpp"
#include "card_pool.hpp"

namespace spire {
// CardLibrary.getCurse iterates its curse-only HashMap, not curseCardPool.
constexpr CardId library_curses[] = {CardId::Regret, CardId::Injury, CardId::Shame, CardId::Parasite, CardId::Normality, CardId::Doubt, CardId::Writhe, CardId::Pain, CardId::Decay, CardId::Clumsy};
SPIRE_FN inline CardId random_curse(Game& g) {
    return library_curses[g.rng.card.random(9)];
}
SPIRE_FN inline void gain_gold(Game& g, int amount) {
    for (int i = 0; i < g.run.relics.size; ++i)
        if (relic_data(g.run.relics[i].id).blocks_gold) return;
    if (amount <= 0) return;
    g.run.gold += amount;
    for (int i = 0; i < g.run.relics.size; ++i) relic_onGainGold(g, i);
}
SPIRE_FN inline void lose_gold(Game& g, int amount) {
    if (g.room_kind == RoomKind::Shop)
        for (int i = 0; i < g.run.relics.size; ++i) relic_onSpendGold(g, i);
    if (amount <= 0) return;
    g.run.gold = max(0, g.run.gold - amount);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onLoseGold(g, i);
}
SPIRE_FN inline void equip_relic(Game& g, RelicId id) {
    if (relic_data(id).stacks_on_obtain)
        for (int i = 0; i < g.run.relics.size; ++i) if (g.run.relics[i].id == id) { ++g.run.relics[i].counter; return; }
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
    for (int i = 0; i < g.reward_objects.size; ++i) {
        auto& object = g.reward_objects[i];
        if (object.relic_slot == remove_index) { object.relic = g.run.relics[remove_index]; object.relic_slot = -1; }
        else if (object.relic_slot > remove_index) --object.relic_slot;
    }
    g.run.relics.erase(remove_index);
    return true;
}
SPIRE_FN inline void preview_card_obtain(Game& g, Card& card) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_onPreviewObtainCard(g, i, card);
}
SPIRE_FN inline void obtain_relic_object(Game& g, int reference) {
    const int size = g.run.relics.size;
    equip_relic(g, g.reward_objects[reference].relic.id);
    if (g.run.relics.size != size) g.reward_objects[reference].relic_slot = size;
}
SPIRE_FN inline int obtain_card(Game& g, Card card) {
    const CardRarity rarity = card_rarity(card.id);
    if (rarity != CardRarity::Basic && rarity != CardRarity::Special && rarity != CardRarity::Curse) {
        SPIRE_ASSERT(g.obtained_cards[static_cast<int>(card.id)] < 65535);
        ++g.obtained_cards[static_cast<int>(card.id)];
    }
    if (card_data(card.id).type == CardType::Curse)
        for (int i = 0; i < g.run.relics.size; ++i) if (relic_onCurseObtain(g, i)) return -1;
    for (int i = 0; i < g.run.relics.size; ++i) relic_onObtainCard(g, i, card);
    const int index = g.run.master_deck.size;
    g.run.master_deck.push_back(card);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onMasterDeckChange(g, i);
    return index;
}
SPIRE_FN inline void obtain_card_object(Game& g, int card) {
    const int master = obtain_card(g, g.cards[card]);
    if (master >= 0) g.cards[card].master_reference = true;
}
SPIRE_FN inline void increase_max_hp(Game& g, int amount) {
    g.player.creature.max_hp += amount;
    heal_creature(g, player_target, amount);
}
SPIRE_FN inline void decrease_max_hp(Game& g, int amount) {
    g.player.creature.max_hp = max(1, g.player.creature.max_hp - amount);
    g.player.creature.hp = min(g.player.creature.hp, g.player.creature.max_hp);
}
SPIRE_FN inline void queue_card_obtain(Game& g, Card card) {
    g.pending_obtains.push_back(create_card(g, card));
}
SPIRE_FN inline void settle_card_obtains(Game& g) {
    for (int i = 0; i < g.pending_obtains.size; ++i) obtain_card_object(g, g.pending_obtains[i]);
    g.pending_obtains.clear();
}
SPIRE_FN inline void open_grid_selection(Game& g, PileId source, int amount, bool for_purge) {
    g.grid_previous_deck_size = get_pile(g, source).size;
    g.screen = Screen::Grid;
    g.grid_source = source;
    g.selection_amount = amount;
    g.selection_any_number = false;
    g.grid_for_purge = for_purge;
    g.grid_for_upgrade = g.grid_for_transform = g.grid_is_master_deck = false;
    g.grid_can_cancel = false;
    g.selected_cards.clear();
}
SPIRE_FN inline void prepare_relic_selection(Game& g) {
    if (g.screen == Screen::CombatReward || g.screen == Screen::Shop || g.screen == Screen::BossReward)
        g.previous_screen = g.screen;
}
SPIRE_FN inline void open_master_selection(Game& g, int amount, bool for_purge) {
    open_grid_selection(g, PileId::MasterDeck, amount, for_purge);
}
SPIRE_FN inline void queue_relic_above(Game& g, int relic, int owner, bool top) {
    if (g.room_phase != RoomPhase::Combat) return;
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
        g.room_rare_card_chance = g.room_kind == RoomKind::Elite ? 10 : 3;
        g.room_uncommon_card_chance = g.room_kind == RoomKind::Elite ? 40 : 37;
        for (int r = 0; r < g.run.relics.size && g.room_kind != RoomKind::Boss; ++r) {
            g.room_rare_card_chance = relic_changeRareCardRewardChance(g, r, g.room_rare_card_chance);
            g.room_uncommon_card_chance = relic_changeUncommonCardRewardChance(g, r, g.room_uncommon_card_chance);
        }
        const int rarity = g.room_kind == RoomKind::Boss || roll < g.room_rare_card_chance ? 2 : roll < g.room_rare_card_chance + g.room_uncommon_card_chance ? 1 : 0;
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
SPIRE_FN inline int create_reward(Game& g, RoomReward reward) {
    if (reward.kind == RewardKind::Relic) {
        reward.relic.counter = relic_data(reward.relic.id).counter;
        relic_initialize_state(reward.relic);
    }
    SPIRE_ASSERT(g.next_reward_identity <= 65535);
    reward.identity = g.next_reward_identity++;
    const int index = g.reward_objects.size;
    g.reward_objects.push_back(reward);
    return index;
}
SPIRE_FN inline int create_library_card(Game& g, int library) {
    const CardId native = library_card_native(library);
    if (native != CardId::Count) return create_card(g, make_card(g, native));
    const int card = card_capacity + g.foreign_reward_cards.size;
    g.foreign_reward_cards.push_back({static_cast<uint16_t>(library), static_cast<uint16_t>(g.next_card_uuid++)});
    return card;
}
SPIRE_FN inline int create_card_reward(Game& g) {
    RoomReward reward{};
    List<const char*, reward_card_capacity> choices{};
    reward_card_choices(g, choices);
    for (int i = 0; i < choices.size; ++i) {
        int library = 0;
        while (!same_id(library_reward_cards[library].id, choices[i])) ++library;
        reward.cards.push_back(create_library_card(g, library));
    }
    for (int i = 0; i < reward.cards.size; ++i) {
        if (reward.cards[i] >= card_capacity) {
            const auto& card = library_reward_cards[g.foreign_reward_cards[reward.cards[i] - card_capacity].library_index];
            if (card.rarity != CardRarity::Rare) SPIRE_ASSERT(!g.rng.card.randomBoolean(g.run.card_upgraded_chance));
            continue;
        }
        Card& card = g.cards[reward.cards[i]];
        if (card_rarity(card.id) != CardRarity::Rare && g.rng.card.randomBoolean(g.run.card_upgraded_chance) &&
            card_canUpgrade(g, reward.cards[i])) card_upgrade(card);
        else preview_card_obtain(g, card);
    }
    return reward.cards.empty() ? -1 : create_reward(g, reward);
}
SPIRE_FN inline void add_room_card_reward(Game& g) {
    const int index = create_card_reward(g);
    if (index >= 0) g.room_rewards.push_back(index);
}
SPIRE_FN inline CardId shuffled_colorless_card(Game& g, CardRarity rarity) {
    shuffle(g.run.colorless_pool.values, g.run.colorless_pool.size, g.rng.shuffle);
    for (int i = 0; i < g.run.colorless_pool.size; ++i) {
        const CardId id = card_id(colorless_card_pool[g.run.colorless_pool[i]].id);
        if (card_rarity(id) == rarity) return id;
    }
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline CardId random_colorless_card(Game& g, CardRarity rarity, bool seeded) {
    List<CardId, 64> choices{};
    for (const auto& entry : colorless_card_pool) {
        const CardId id = card_id(entry.id);
        if (card_rarity(id) != rarity) continue;
        int at = choices.size;
        while (at > 0 && card_sort_order(choices[at - 1]) > card_sort_order(id)) --at;
        choices.insert(at, id);
    }
    return choices[seeded ? g.rng.card.random(choices.size - 1) : g.unseeded.random(choices.size - 1)];
}
SPIRE_FN inline void reward_gold_bonus(const Game& g, RoomReward& reward) {
    reward.bonus_gold = 0;
    if (g.room_kind != RoomKind::Treasure && reward.kind == RewardKind::Gold)
        for (int i = 0; i < g.run.relics.size; ++i)
            reward.bonus_gold += floor_int(reward.amount * relic_data(g.run.relics[i].id).gold_reward_bonus + 0.5f);
}
SPIRE_FN inline void add_room_gold(Game& g, int amount, RewardKind kind) {
    for (int i = 0; i < g.room_rewards.size; ++i) {
        auto& reward = g.reward_objects[g.room_rewards[i]];
        if (reward.kind != kind) continue;
        reward.amount += amount;
        reward_gold_bonus(g, reward);
        return;
    }
    RoomReward reward{};
    reward.kind = kind;
    reward.amount = amount;
    reward_gold_bonus(g, reward);
    g.room_rewards.push_back(create_reward(g, reward));
}
SPIRE_FN inline void add_gold_reward(Game& g, int amount) { add_room_gold(g, amount, RewardKind::Gold); }
SPIRE_FN inline void add_stolen_gold_reward(Game& g, int amount) { add_room_gold(g, amount, RewardKind::StolenGold); }
SPIRE_FN inline void add_potion_reward(Game& g, PotionId potion) {
    RoomReward reward{};
    reward.kind = RewardKind::Potion;
    reward.potion = potion;
    reward.potion_state = construct_potion(g, potion);
    g.room_rewards.push_back(create_reward(g, reward));
}
SPIRE_FN inline void open_room_card_rewards(Game& g) {
    g.room_reward_time = true;
    g.screen_rewards = g.room_rewards;
    if (g.room_kind != RoomKind::Rest && g.room_kind != RoomKind::Treasure && !g.event.no_cards) {
        const int card = create_card_reward(g);
        if (card >= 0) g.screen_rewards.push_back(card);
        if (g.room_kind == RoomKind::Monster)
            for (int i = 0; i < g.run.relics.size; ++i) if (relic_data(g.run.relics[i].id).extra_normal_combat_card_reward) {
                const int extra = create_card_reward(g);
                if (extra >= 0) g.screen_rewards.push_back(extra);
                break;
            }
    }
    g.reward_proceed_visible = false;
    g.combat_reward_skippable = true;
    g.reward_skippable = true;
    g.screen = Screen::CombatReward;
}
SPIRE_FN inline void open_combat_rewards(Game& g) {
    open_room_card_rewards(g);
    g.reward_proceed_visible = true;
    g.combat_reward_skippable = false;
}
SPIRE_FN inline void remove_room_relic_reward(Game& g) {
    for (int i = 0; i < g.room_rewards.size; ++i) {
        const RoomReward& reward = g.reward_objects[g.room_rewards[i]];
        if (reward.relic.id == RelicId::Count) continue;
        const int link = reward.relic_link;
        g.room_rewards.erase(i);
        if (i < g.room_rewards.size && link == g.reward_objects[g.room_rewards[i]].identity) g.room_rewards.erase(i);
        break;
    }
}
SPIRE_FN inline void remove_master_card(Game& g, int index) {
    Card removed = g.run.master_deck.erase(index);
    bool referenced = g.event.id != EventId::Count && event_referencesMasterCard(g, removed.uuid);
    for (int i = 0; i < g.run.relics.size; ++i) referenced |= relic_referencesMasterCard(g, i, removed.uuid);
    int retained = -1;
    for (int i = 0; i < g.cards.size; ++i)
        if (g.cards[i].master_reference && g.cards[i].uuid == removed.uuid) { retained = i; break; }
    if (retained >= 0) {
        g.cards[retained] = removed;
        g.cards[retained].master_reference = true;
        g.detached_master_cards.push_back(retained);
    } else if (referenced) {
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
    const CardPoolEntry* pools[] = {common_card_pool, uncommon_card_pool, rare_card_pool, colorless_card_pool};
    const int sizes[] = {static_cast<int>(sizeof(common_card_pool) / sizeof(*common_card_pool)),
                         static_cast<int>(sizeof(uncommon_card_pool) / sizeof(*uncommon_card_pool)),
                         static_cast<int>(sizeof(rare_card_pool) / sizeof(*rare_card_pool)),
                         static_cast<int>(sizeof(colorless_card_pool) / sizeof(*colorless_card_pool))};
    List<CardId, 128> candidates{};
    const CardRarity rarity = card_rarity(original.id);
    const bool colorless = card_colorless(original.id);
    if (rarity == CardRarity::Curse) {
        for (const CardId id : library_curses) if (id != original.id) candidates.push_back(id);
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
        while (g.run.relic_pools[pool].empty() && (pool < 2 || pool == 3)) {
            pool = pool == 3 ? 1 : pool + 1;
            from_end = false;
        }
        RelicKey key;
        if (g.run.relic_pools[pool].empty()) {
            // RelicLibrary.getRelic maps AbstractDungeon's exhausted boss-pool key to Circlet.
            key = RelicKey::Circlet;
        }
        else key = g.run.relic_pools[pool].erase((from_end && pool != static_cast<int>(RelicTier::Boss)) ? g.run.relic_pools[pool].size - 1 : 0);
        if (relic_can_spawn_id(g, relic_id(relic_key_name(key)))) return key;
        from_end = true;
    }
}
SPIRE_FN inline void add_relic_reward(Game& g, RelicTier tier) {
    RoomReward reward{};
    reward.kind = RewardKind::Relic;
    reward.relic.id = relic_id(relic_key_name(random_relic_key(g, static_cast<int>(tier))));
    g.room_rewards.push_back(create_reward(g, reward));
}
SPIRE_FN inline void settle_relic_acquisition(Game& g, RandomXS128& policy) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_update(g, i);
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
