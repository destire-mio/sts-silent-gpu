#pragma once
#include "cards.hpp"
#include "card_pool.hpp"

namespace spire {
SPIRE_FN inline void gain_gold(Game& g, int amount) {
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
    for (int i = 0; i < g.run.relics.size; ++i) relic_onObtainCard(g, i, card);
    g.run.master_deck.push_back(card);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onMasterDeckChange(g, i);
}
SPIRE_FN inline void show_card_obtain(Game& g, Card card) {
    const CardRarity rarity = card_rarity(card.id);
    if (rarity != CardRarity::Basic && rarity != CardRarity::Special && rarity != CardRarity::Curse)
        ++g.obtained_cards[static_cast<int>(card.id)];
    if (card_data(card.id).type == CardType::Curse) {
        for (int i = 0; i < g.run.relics.size; ++i) {
            if (relic_onCurseObtain(g, i)) return;
        }
    }
    obtain_card(g, card);
}
SPIRE_FN inline void queue_relic_above(Game& g, int relic, int owner, bool top) {
    Effect e{};
    e.kind = EffectKind::RelicAboveCreature;
    action_initialize_state(e);
    e.source = e.target = owner;
    e.state.RelicAboveCreature.relic = relic;
    if (top) add_top(g, e);
    else add_bottom(g, e);
}
SPIRE_FN inline void add_room_card_reward(Game& g) {
    RoomReward reward{};
    reward.identity = g.next_reward_identity++;
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
        CardId id;
        bool duplicate;
        do {
            id = card_id(pools[rarity][g.rng.card.random(sizes[rarity] - 1)].id);
            duplicate = false;
            for (int c = 0; c < reward.cards.size; ++c) if (g.cards[reward.cards[c]].id == id) duplicate = true;
        } while (duplicate);
        reward.cards.push_back(create_card(g, make_card(id)));
    }
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
    card_onRemoveFromMasterDeck(g, removed);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onMasterDeckChange(g, i);
}
SPIRE_FN inline void on_victory(Game& g) {
    if (!g.player.creature.dying) {
        for (int i = 0; i < g.run.relics.size; ++i) relic_onVictory(g, i);
        Powers& powers = g.player.creature.powers;
        for (int i = 0; i < powers.size; ++i) power_onVictory(g, powers.at(g.power_objects, i), player_target);
    }
}
SPIRE_FN inline void gain_energy(Game& g, int amount) {
    g.player.energy = min(999, g.player.energy + amount);
}
}
