#pragma once
#include "cards.hpp"
#include "monsters.hpp"
#include "campfire.hpp"
#include "treasure.hpp"
#include "shop.hpp"
#include "event_room.hpp"
#include "card_pool.hpp"
#include "dungeon.hpp"
#include "orbs.hpp"
#include "stances.hpp"

namespace spire {
SPIRE_FN inline bool potion_can_discard_base(const Game& g, int) {
    return g.event.id == EventId::Count || !event_data(g.event.id).blocks_potions;
}
SPIRE_FN inline bool potion_can_use_base(const Game& g, int slot) {
    return potion_canDiscard(g, slot) && g.room_in_combat && !monsters_basically_dead(g) && !g.manager.turn_has_ended;
}
SPIRE_FN inline void insert_random_card(Game& g, Pile& pile, int card) {
    if (pile.empty()) pile.push_back(card);
    else pile.insert(g.rng.card_random.random(pile.size - 1), card);
}
SPIRE_FN inline int copy_card(Game& g, int source, bool same_uuid) {
    const int index = g.cards.size;
    Card card = card_makeStatEquivalentCopy(g, source);
    card.uuid = same_uuid ? g.cards[source].uuid : g.next_card_uuid++;
    g.cards.push_back(card);
    return index;
}
SPIRE_FN inline void remove_queued_card(Game& g, int card) {
    const int count = g.manager.card_queue.size;
    bool removed = false;
    for (int i = 0; i < count; ++i) {
        const CardQueueItem item = g.manager.card_queue.pop_front();
        if (!removed && item.card == card) removed = true;
        else g.manager.card_queue.push_back(item);
    }
}
SPIRE_FN inline int create_card(Game& g, Card card) {
    const int index = g.cards.size;
    card.uuid = g.next_card_uuid++;
    g.cards.push_back(card);
    return index;
}
SPIRE_FN inline int upgrade_name_order(int count) {
    int order = 0, place = 1;
    do { order += (count % 10 + 1) * place; place *= 11; count /= 10; } while (count);
    while (place < 161051) { order *= 11; place *= 11; }
    return order;
}
SPIRE_FN inline void sort_selection_cards(Game& g) {
    const int rarity_rank[] = {0, 2, 3, 4, 1, 5};
    for (int i = 1; i < g.grid_cards.size; ++i) {
        const int card = g.grid_cards[i]; const Card& value = g.cards[card];
        const int rank = (card_data(value.id).type == CardType::Status ? 100000 : 0) - rarity_rank[static_cast<int>(card_rarity(value.id))] * 1000 + card_name_order(value);
        int j = i;
        while (j > 0) {
            const Card& previous = g.cards[g.grid_cards[j - 1]];
            const int previous_rank = (card_data(previous.id).type == CardType::Status ? 100000 : 0) - rarity_rank[static_cast<int>(card_rarity(previous.id))] * 1000 + card_name_order(previous);
            if (previous_rank < rank) break;
            if (previous_rank == rank && (previous.id != value.id || !previous.upgrades || !value.upgrades ||
                upgrade_name_order(previous.upgrades) <= upgrade_name_order(value.upgrades))) break;
            g.grid_cards[j] = g.grid_cards[j - 1]; --j;
        }
        g.grid_cards[j] = card;
    }
}
SPIRE_FN inline CardId foreign_attack_card(Game& g) {
    const int roll = g.rng.card_random.random(99);
    const CardRarity rarity = roll < 55 ? CardRarity::Common : roll < 85 ? CardRarity::Uncommon : CardRarity::Rare;
    List<uint16_t, 384> cards{};
    for (int i = 0; i < static_cast<int>(sizeof(library_reward_cards) / sizeof(*library_reward_cards)); ++i)
        if (library_reward_cards[i].type == CardType::Attack && library_reward_cards[i].rarity == rarity && !library_reward_cards[i].healing) cards.push_back(i);
    g.rng.card_random.randomLong();
    return card_id(library_reward_cards[cards[g.rng.card.random(cards.size - 1)]].id);
}
SPIRE_FN inline CardId random_common_card(Game& g) {
    return card_id(common_card_pool[g.rng.card_random.random(static_cast<int>(sizeof(common_card_pool) / sizeof(*common_card_pool)) - 1)].id);
}
SPIRE_FN inline const char* random_card_in_combat(Game& g, int type, bool colorless) {
    const CardPoolEntry* pools[] = {common_card_pool, uncommon_card_pool, rare_card_pool, colorless_card_pool};
    List<const char*, 128> candidates{};
    for (int p = colorless ? 3 : 0; p < (colorless ? 4 : 3); ++p) {
        for (int i = 0; i < g.run.source_card_pools[p].size; ++i) {
            const CardPoolEntry& card = pools[p][g.run.source_card_pools[p][i]];
            if (!card.healing && (colorless || type < 0 || static_cast<int>(card.type) == type))
                candidates.push_back(card.id);
        }
    }
    return candidates[g.rng.card_random.random(candidates.size - 1)];
}
SPIRE_FN inline PotionState construct_potion(const Game& g, PotionId id) {
    PotionState state{};
    potion_constructPotion(g, state, id);
    return state;
}
SPIRE_FN inline PotionId random_potion_from_pool(Game&, Random& rng) {
    const char* id = silent_potion_pool[rng.random(static_cast<int>(sizeof(silent_potion_pool) / sizeof(*silent_potion_pool)) - 1)].id;
    for (int i = 0; i < static_cast<int>(PotionId::Count); ++i)
        if (potion_matches_id(static_cast<PotionId>(i), id)) return static_cast<PotionId>(i);
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline PotionId random_potion(Game& g, bool limited) {
    const int roll = g.rng.potion.random(0, 99);
    const int rarity = roll < 65 ? 0 : roll < 90 ? 1 : 2;
    int index = g.rng.potion.random(32);
    bool spam_check = limited;
    while (silent_potion_pool[index].rarity != rarity || spam_check) {
        spam_check = limited;
        index = g.rng.potion.random(32);
        if (silent_potion_pool[index].limited_exclusion) continue;
        spam_check = false;
    }
    for (int i = 0; i < static_cast<int>(PotionId::Count); ++i)
        if (potion_matches_id(static_cast<PotionId>(i), silent_potion_pool[index].id)) return static_cast<PotionId>(i);
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline PotionId potion_from_pool(Random& rng) {
    const char* name = silent_potion_pool[rng.random(32)].id;
    for (int i = 0; i < static_cast<int>(PotionId::Count); ++i)
        if (potion_matches_id(static_cast<PotionId>(i), name)) return static_cast<PotionId>(i);
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline void obtain_potion(Game& g, PotionId id, const PotionState& state) {
    for (int i = 0; i < g.run.potion_slots; ++i) {
        if (g.run.potions[i] != PotionId::Empty) continue;
        g.run.potions[i] = id;
        g.run.potion_identity[i] = 0;
        g.run.potion_state[i] = state;
        return;
    }
}
SPIRE_FN inline void move_to_bottom_of_deck(Game& g, int card) {
    remove_queued_card(g, card);
    remove_from_pile(g.hand, card);
    g.draw.insert(0, card);
    mark_card_move(g, card);
}
SPIRE_FN inline void upgrade_created_card(Game& g, int card) {
    const CardType type = card_data(g.cards[card].id).type;
    if (type != CardType::Curse && type != CardType::Status && has_power(g, g.player.creature, PowerId::MasterReality)) card_upgrade(g.cards[card]);
}
SPIRE_FN inline void add_created_card_to_hand(Game& g, int card) {
    upgrade_created_card(g, card);
    g.hand.push_back(card);
    card_triggerWhenCopied(g, card);
    for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
    on_draw_or_discard(g);
}
SPIRE_FN inline void refresh_hand(Game& g) {
    if (monsters_basically_dead(g)) return;
    const Powers& powers = g.player.creature.powers;
    for (int p = 0; p < powers.size; ++p) {
        const PowerId rear = power_data(powers.at(g.power_objects, p).id).rear_attack_power;
        if (rear == PowerId::Count) continue;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int owner = g.monster_order[i];
            monster_applyPowers(g, owner);
            if (rear_damage_multiplier(g, owner) != 1.0f || !has_power(g, g.monsters[owner].creature, rear)) continue;
            Effect e{}; e.kind = EffectKind::RemovePower; e.target = owner; e.source = no_target; e.power = rear;
            add_top(g, e);
        }
    }
    for (int i = 0; i < g.run.relics.size; ++i) relic_onRefreshHand(g, i);
}
SPIRE_FN inline void on_draw_or_discard(Game& g) {
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_onDrawOrDiscard(g, powers.at(g.power_objects, i), player_target);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onDrawOrDiscard(g, i);
    if (has_power(g, g.player.creature, PowerId::Corruption))
        for (int i = 0; i < g.hand.size; ++i) {
            Card& card = g.cards[g.hand[i]];
            if (card_data(card.id).type == CardType::Skill && card.cost_for_turn != 0) modify_cost_for_combat(card, -9);
        }
    for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
}
SPIRE_FN inline void move_to_discard(Game& g, int card) {
    remove_queued_card(g, card);
    mark_card_move(g, card);
    card_onMoveToDiscard(g, card);
    g.discard.push_back(card);
    on_draw_or_discard(g);
}
SPIRE_FN inline Pile& get_pile(Game& g, PileId pile) {
    switch (pile) {
    case PileId::Hand: return g.hand;
    case PileId::Draw: return g.draw;
    case PileId::Discard: return g.discard;
    case PileId::Exhaust: return g.exhaust;
    case PileId::Limbo: return g.limbo;
    case PileId::Grid: case PileId::MasterDeck: return g.grid_cards;
    }
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline const Pile& get_pile(const Game& g, PileId pile) {
    switch (pile) {
    case PileId::Hand: return g.hand;
    case PileId::Draw: return g.draw;
    case PileId::Discard: return g.discard;
    case PileId::Exhaust: return g.exhaust;
    case PileId::Limbo: return g.limbo;
    case PileId::Grid: case PileId::MasterDeck: return g.grid_cards;
    }
    SPIRE_ASSERT(false);
    __builtin_unreachable();
}
SPIRE_FN inline bool remove_from_pile(Pile& pile, int card) {
    for (int i = 0; i < pile.size; ++i) {
        if (pile[i] == card) { pile.erase(i); return true; }
    }
    return false;
}
SPIRE_FN inline void open_hand_selection(Game& g, int amount, bool any_number, bool can_pick_zero, bool for_upgrade, bool up_to) {
    const int queued = g.manager.card_queue.size;
    for (int i = 0; i < queued; ++i) {
        const CardQueueItem item = g.manager.card_queue.pop_front();
        bool in_hand = false;
        for (int j = 0; j < g.hand.size; ++j) if (g.hand[j] == item.card) in_hand = true;
        if (!in_hand) g.manager.card_queue.push_back(item);
    }

    g.discard_remaining = amount;
    g.selection_amount = amount;
    g.selection_for_upgrade = for_upgrade;
    g.selection_up_to = up_to;
    g.selection_any_number = any_number;
    g.selection_can_pick_zero = can_pick_zero;
    g.selected_cards.clear();
    g.selected_cards_retrieved = false;
    g.screen = Screen::Discard;
}
SPIRE_FN inline void discard_card(Game& g, int hand_index, bool end_turn) {
    const int card = g.hand.erase(hand_index);
    move_to_discard(g, card);
    if (!end_turn) card_triggerOnManualDiscard(g, card);
    increment_discard(g, end_turn);
}
SPIRE_FN inline void increment_discard(Game& g, bool end_turn) {
    ++g.manager.total_discarded_this_turn;
    if (!end_turn && !g.manager.turn_has_ended) {
        for (int i = 0; i < g.hand.size; ++i) card_didDiscard(g, g.hand[i]);
        for (int i = 0; i < g.discard.size; ++i) card_didDiscard(g, g.discard[i]);
        for (int i = 0; i < g.draw.size; ++i) card_didDiscard(g, g.draw[i]);
        for (int i = 0; i < g.run.relics.size; ++i) relic_onManualDiscard(g, i);
    }
}
SPIRE_FN inline void exhaust_from_pile(Game& g, int card, PileId pile) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_onExhaust(g, i, card);
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_onExhaust(g, powers.at(g.power_objects, i), player_target, card);
    card_triggerOnExhaust(g, card);
    remove_queued_card(g, card);
    card_resetAttributes(g, card);
    remove_from_pile(get_pile(g, pile), card);
    g.exhaust.push_back(card);
    on_draw_or_discard(g);
}
SPIRE_FN inline void draw_pile_to_discard(Game& g, int card) {
    remove_queued_card(g, card);
    remove_from_pile(g.draw, card);
    mark_card_move(g, card);
    card_onMoveToDiscard(g, card);
    g.discard.push_back(card);
    on_draw_or_discard(g);
}
SPIRE_FN inline void hand_to_deck(Game& g, int card) {
    remove_queued_card(g, card);
    remove_from_pile(g.hand, card);
    g.draw.push_back(card);
    mark_card_move(g, card);
}
SPIRE_FN inline void add_card_queue_item(Game& g, const CardQueueItem& item, bool immediate) {
    // GameActionManager.addCardQueueItem inserts after the currently queued card.
    if (immediate && !g.manager.card_queue.empty()) g.manager.card_queue.insert(1, item);
    else g.manager.card_queue.push_back(item);
}
SPIRE_FN inline void exhaust_card(Game& g, int card) { exhaust_from_pile(g, card, PileId::Hand); }
SPIRE_FN inline void clear_post_combat_actions(Game& g) {
    const int count = g.manager.actions.size;
    for (int i = 0; i < count; ++i) {
        const int index = g.manager.actions.pop_front();
        const Effect& e = g.action_objects[index];
        if (action_data(e.kind).survives_combat_end)
            g.manager.actions.push_back(index);
        else if (!e.held) release_action(g, index);
    }
}
SPIRE_FN inline int decrement_block(Game& g, int owner, const DamageInfo& info, int amount) {
    Creature& target = get_creature(g, owner);
    if (info.damage_type != DamageType::HpLoss && target.block > 0) {
        const int blocked = min(target.block, amount);
        target.block -= blocked;
        amount -= blocked;
        if (target.block == 0 && owner != player_target)
            for (int i = 0; i < g.run.relics.size; ++i) relic_onBlockBroken(g, i, owner);
    }
    return amount;
}
SPIRE_FN inline void monster_die_base(Game& g, int owner, bool trigger_relics) {
    Creature& c = g.monsters[owner].creature;
    if (c.dying) return;
    c.dying = true;
    if (trigger_relics) {
        if (c.hp <= 0)
            for (int i = 0; i < c.powers.size; ++i) power_onDeath(g, c.powers.at(g.power_objects, i), owner);
        for (int i = 0; i < g.run.relics.size; ++i) relic_onMonsterDeath(g, i, owner);
    }
    c.hp = max(0, c.hp);
    c.dead = true;
}
SPIRE_FN inline void monster_damage_base(Game& g, int owner, const DamageInfo& info) {
    Creature& target = g.monsters[owner].creature;
    if (target.dying || target.escaped) return;
    int amount = decrement_block(g, owner, info, max(0, info.amount));
    if (info.source == player_target)
        for (int i = 0; i < g.run.relics.size; ++i) amount = relic_onAttackToChangeDamage(g, i, info, amount);
    if (info.source != no_target) {
        Powers& source = get_creature(g, info.source).powers;
        for (int i = 0; i < source.size; ++i) amount = power_onAttackToChangeDamage(g, source.at(g.power_objects, i), info.source, info, amount);
    }
    for (int i = 0; i < target.powers.size; ++i) amount = power_onAttackedToChangeDamage(g, target.powers.at(g.power_objects, i), owner, info, amount);
    if (info.source == player_target)
        for (int i = 0; i < g.run.relics.size; ++i) relic_onAttack(g, i, info, amount, owner);
    for (int i = 0; i < target.powers.size; ++i) power_wasHPLost(g, target.powers.at(g.power_objects, i), owner, info, amount);
    if (info.source != no_target) {
        Powers& source = get_creature(g, info.source).powers;
        for (int i = 0; i < source.size; ++i) power_onAttack(g, source.at(g.power_objects, i), info.source, info, amount, owner);
    }
    for (int i = 0; i < target.powers.size; ++i) amount = power_onAttacked(g, target.powers.at(g.power_objects, i), owner, info, amount);
    target.last_damage_taken = min(amount, target.hp);
    if (amount > 0) target.hp = max(0, target.hp - amount);
    if (target.hp <= 0) {
        monster_die(g, owner, true);
        target.block = 0;
    }
}
SPIRE_FN inline void player_damage(Game& g, const DamageInfo& info) {
    Creature& target = g.player.creature;
    int amount = max(0, info.amount);
    // AbstractPlayer.damage caps uncalculated DamageInfo before block, including HP_LOSS.
    for (int i = 0; i < target.powers.size; ++i) {
        const int cap = power_data(target.powers.at(g.power_objects, i).id).incoming_damage_limit;
        if (cap > 0) amount = min(amount, cap);
    }
    amount = decrement_block(g, player_target, info, amount);
    if (info.source == player_target)
        for (int i = 0; i < g.run.relics.size; ++i) amount = relic_onAttackToChangeDamage(g, i, info, amount);
    if (info.source != no_target) {
        Powers& source = get_creature(g, info.source).powers;
        for (int i = 0; i < source.size; ++i) amount = power_onAttackToChangeDamage(g, source.at(g.power_objects, i), info.source, info, amount);
    }
    for (int i = 0; i < g.run.relics.size; ++i) amount = relic_onAttackedToChangeDamage(g, i, info, amount);
    for (int i = 0; i < target.powers.size; ++i) amount = power_onAttackedToChangeDamage(g, target.powers.at(g.power_objects, i), player_target, info, amount);
    if (info.source == player_target)
        for (int i = 0; i < g.run.relics.size; ++i) relic_onAttack(g, i, info, amount, player_target);
    if (info.source != no_target) {
        Powers& source = get_creature(g, info.source).powers;
        for (int i = 0; i < source.size; ++i) power_onAttack(g, source.at(g.power_objects, i), info.source, info, amount, player_target);
        for (int i = 0; i < target.powers.size; ++i) amount = power_onAttacked(g, target.powers.at(g.power_objects, i), player_target, info, amount);
        for (int i = 0; i < g.run.relics.size; ++i) amount = relic_onAttacked(g, i, info, amount);
    }
    for (int i = 0; i < g.run.relics.size; ++i) amount = relic_onLoseHpLast(g, i, amount);
    target.last_damage_taken = min(amount, target.hp);
    if (amount > 0) {
        for (int i = 0; i < target.powers.size; ++i) amount = power_onLoseHp(g, target.powers.at(g.power_objects, i), player_target, amount);
        for (int i = 0; i < g.run.relics.size; ++i) relic_onLoseHp(g, i, amount);
        for (int i = 0; i < target.powers.size; ++i) power_wasHPLost(g, target.powers.at(g.power_objects, i), player_target, info, amount);
        for (int i = 0; i < g.run.relics.size; ++i) relic_wasHPLost(g, i, amount);
        if (info.source != no_target) {
            Powers& source = get_creature(g, info.source).powers;
            for (int i = 0; i < source.size; ++i) power_onInflictDamage(g, source.at(g.power_objects, i), info.source, info, amount, player_target);
        }
        g.manager.damage_received_this_turn += amount;
        g.manager.damage_received_this_combat += amount;
        if (info.damage_type == DamageType::HpLoss) g.manager.hp_loss_this_combat += amount;
        target.hp -= amount;
        if (amount > 0) {
            for (int i = 0; i < g.hand.size; ++i) card_tookDamage(g, g.hand[i]);
            for (int i = 0; i < g.discard.size; ++i) card_tookDamage(g, g.discard[i]);
            for (int i = 0; i < g.draw.size; ++i) card_tookDamage(g, g.draw[i]);
        }
        target.hp = max(0, target.hp);
        if (target.hp <= target.max_hp / 2 && !g.player.bloodied) {
            g.player.bloodied = true;
            for (int i = 0; i < g.run.relics.size; ++i) relic_onBloodied(g, i);
        }
        if (target.hp == 0) {
            bool prevents_revival = false;
            for (int i = 0; i < g.run.relics.size; ++i) prevents_revival |= relic_data(g.run.relics[i].id).prevents_revival;
            if (!prevents_revival) {
                for (int slot = 0; slot < g.run.potion_slots; ++slot) {
                    if (!potion_data(g.run.potions[slot]).revives_player) continue;
                    potion_use(g, slot, player_target);
                    g.run.potions[slot] = PotionId::Empty;
                    return;
                }
                for (int i = 0; i < g.run.relics.size; ++i) {
                    const RelicData data = relic_data(g.run.relics[i].id);
                    if (data.revives_player && g.run.relics[i].counter == data.counter) {
                        relic_onTrigger(g, i);
                        return;
                    }
                }
            }
            target.dead = true;
            target.block = 0;
            g.screen = Screen::Death;
            // DeathScreen.<init> clears event dialogs inside the damage callback.
            if (g.event.id != EventId::Count) { g.image_dialog.clear(); g.room_dialog.clear(); }
        }
    }
}
SPIRE_FN inline void creature_damage(Game& g, const DamageInfo& e) {
    if (e.target == player_target) player_damage(g, e);
    else {
        monster_damage(g, e.target, e);
        if (monsters_basically_dead(g)) {
            if (!g.cannot_lose) g.battle_over = true;
            g.manager.card_queue.clear();
        }
    }
}
SPIRE_FN inline void damage(Game& g, const Effect& e) {
    creature_damage(g, e);
    if (e.target != player_target && monsters_basically_dead(g)) clear_post_combat_actions(g);
}
SPIRE_FN inline void heal_creature(Game& g, int owner, int amount) {
    Creature& c = get_creature(g, owner);
    if (c.dying) return;
    if (owner == player_target)
        for (int i = 0; i < g.run.relics.size; ++i) amount = relic_onPlayerHeal(g, i, amount);
    for (int i = 0; i < c.powers.size; ++i) amount = power_onHeal(g, c.powers.at(g.power_objects, i), owner, amount);
    c.hp = min(c.max_hp, c.hp + amount);
    if (owner == player_target && c.hp > c.max_hp / 2 && g.player.bloodied) {
        g.player.bloodied = false;
        for (int i = 0; i < g.run.relics.size; ++i) relic_onNotBloodied(g, i);
    }
}
SPIRE_FN inline void monster_apply_powers(Game& g, int owner) {
    Monster& m = g.monsters[owner];
    const float rear_multiplier = rear_damage_multiplier(g, owner);
    if (rear_multiplier != 1.0f) {
        const Powers& powers = g.player.creature.powers;
        for (int i = 0; i < powers.size; ++i) {
            const PowerId id = power_data(powers.at(g.power_objects, i).id).rear_attack_power;
            if (id == PowerId::Count || has_power(g, m.creature, id)) continue;
            Effect e{};
            e.kind = EffectKind::ApplyPower;
            e.source = no_target;
            e.target = owner;
            e.power = id;
            e.amount = 1;
            add_top(g, e);
        }
    }
    const Powers& source = m.creature.powers;
    const Powers& target = g.player.creature.powers;
    for (int d = 0; d < m.damage_count; ++d) {
        const DamageType type = monster_data(m.id).damage_types[d];
        float value = static_cast<float>(monster_base_damage(g, m, d));
        bool modified = false;
        for (int i = 0; i < source.size; ++i) {
            value = power_atDamageGive(g, source.at(g.power_objects, i), owner, value, type, -1);
            if (monster_base_damage(g, m, d) != static_cast<int>(value)) modified = true;
        }
        for (int i = 0; i < target.size; ++i) {
            value = power_atDamageReceive(g, target.at(g.power_objects, i), player_target, value, type, -1);
            if (monster_base_damage(g, m, d) != static_cast<int>(value)) modified = true;
        }
        value = stance_damage_receive(g, value, type);
        if (monster_base_damage(g, m, d) != static_cast<int>(value)) modified = true;
        for (int i = 0; i < source.size; ++i) {
            value = power_atDamageFinalGive(g, source.at(g.power_objects, i), owner, value, type, -1);
            if (monster_base_damage(g, m, d) != static_cast<int>(value)) modified = true;
        }
        for (int i = 0; i < target.size; ++i) {
            value = power_atDamageFinalReceive(g, target.at(g.power_objects, i), player_target, value, type, -1);
            if (monster_base_damage(g, m, d) != static_cast<int>(value)) modified = true;
        }
        monster_output_damage(g, m, d) = static_cast<int>(max(0, floor_int(value)) * rear_multiplier);
        m.damage_modified = (m.damage_modified & ~(1u << d)) | (static_cast<unsigned>(modified) << d);
    }
    if (m.pending_base_damage >= 0) m.intent_damage = calculate_intent_damage(g, owner, m.pending_base_damage);
}
SPIRE_FN inline void mark_card_move(Game& g, int card) {
    g.pending_card_moves[card / 64] |= uint64_t{1} << (card % 64);
}
SPIRE_FN inline void settle_card_moves(Game& g) {
    // Soul.update clears the moving card and refreshes the hand when its move completes.
    bool moved = false;
    for (int word = 0; word < (card_capacity + 63) / 64; ++word) {
        while (g.pending_card_moves[word]) {
            const int bit = __builtin_ctzll(g.pending_card_moves[word]);
            card_clearPowers(g, word * 64 + bit);
            g.pending_card_moves[word] &= ~(uint64_t{1} << bit);
            moved = true;
        }
    }
    if (moved && g.screen != Screen::Discard && g.screen != Screen::Grid && g.screen != Screen::CardReward)
        for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
}
SPIRE_FN inline void draw_cards(Game& g, Effect& effect) {
    // DrawCardAction.update waits for SoulGroup.isActive to become false.
    settle_card_moves(g);
    if (!effect.flag) { g.drawn_cards.clear(); effect.flag = true; }
    bool blocked = false;
    for (int i = 0; i < g.player.creature.powers.size; ++i)
        blocked |= power_data(g.player.creature.powers.at(g.power_objects, i).id).blocks_draw;
    if (blocked || effect.amount <= 0 ||
        g.draw.size + g.discard.size == 0 || g.hand.size == hand_limit) {
        effect.done = true;
        if (effect.payload.state.Draw.follow_up_action != 0)
            g.manager.actions.push_front(effect.payload.state.Draw.follow_up_action - 1);
        return;
    }
    if (!effect.payload.state.Draw.shuffle_check) {
        if (effect.amount + g.hand.size > hand_limit) effect.amount = hand_limit - g.hand.size;
        if (effect.amount > g.draw.size) {
            Effect tail = units::actions::Draw::construct(effect.amount - g.draw.size, false,
                                                         effect.payload.state.Draw.follow_up_action - 1);
            add_top(g, tail);
            Effect shuffle_effect{};
            shuffle_effect.kind = EffectKind::Shuffle;
            add_top(g, shuffle_effect);
            if (!g.draw.empty()) add_top(g, units::actions::Draw::construct(g.draw.size, false));
            effect.amount = 0;
            effect.done = true;
            return;
        }
        effect.payload.state.Draw.shuffle_check = true;
    }
    --effect.amount;
    if (!g.draw.empty()) {
        const int card = g.draw[g.draw.size - 1];
        g.drawn_cards.push_back(card);
        card_triggerWhenDrawn(g, card);
        g.hand.push_back(card);
        g.draw.pop_back();
        Powers& powers = g.player.creature.powers;
        for (int j = 0; j < powers.size; ++j) power_onCardDraw(g, powers.at(g.power_objects, j), player_target, card);
        for (int j = 0; j < g.run.relics.size; ++j) relic_onCardDraw(g, j, card);
        on_draw_or_discard(g);
        refresh_hand(g);
    } else {
        effect.done = true;
        if (effect.payload.state.Draw.follow_up_action != 0)
            g.manager.actions.push_front(effect.payload.state.Draw.follow_up_action - 1);
    }
    if (effect.amount == 0) {
        effect.done = true;
        if (effect.payload.state.Draw.follow_up_action != 0)
            g.manager.actions.push_front(effect.payload.state.Draw.follow_up_action - 1);
    }
}
SPIRE_FN inline void execute_effect(Game& g, Effect& e) {
    if (e.done) return;
    action_execute(g, e);
    if (!action_data(e.kind).multi_step && g.screen != Screen::Discard &&
        g.screen != Screen::Grid && g.screen != Screen::CardReward) e.done = true;
}
SPIRE_FN inline void end_turn_early(Game& g) {
    g.player.end_turn_queued = true;
    for (int i = 0; i < g.manager.card_queue.size; ++i) {
        const CardQueueItem& item = g.manager.card_queue[i];
        if (!item.autoplay_card) continue;
        g.cards[item.card].dont_trigger_on_use_card = true;
        add_bottom(g, units::actions::UseCard::construct(g, item.card, no_target));
    }
    g.manager.card_queue.clear();
    g.limbo.clear();
    add_bottom(g, units::actions::NewQueueCard::construct());
}
SPIRE_FN inline void begin_end_turn(Game& g) {
    g.player.end_turn_queued = true;
    for (int i = 0; i < g.run.relics.size; ++i) relic_onPlayerEndTurn(g, i);
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_atEndOfTurnPreEndTurnCards(g, powers.at(g.power_objects, i), player_target, true);
    Effect orbs{};
    orbs.kind = EffectKind::TriggerEndOfTurnOrbs;
    add_bottom(g, orbs);
    for (int i = 0; i < g.hand.size; ++i) card_triggerOnEndOfTurnForPlayingCard(g, g.hand[i]);
}
SPIRE_FN inline void complete_end_turn(Game& g) {
    g.player.end_turn_queued = false;
    at_end_of_turn(g, player_target);
    Effect clear{};
    clear.kind = EffectKind::ClearCardQueue;
    add_bottom(g, clear);
    Effect discard{};
    discard.kind = EffectKind::EndTurnDiscard;
    add_bottom(g, discard);
    for (int i = 0; i < g.draw.size; ++i) card_resetAttributes(g, g.draw[i]);
    for (int i = 0; i < g.discard.size; ++i) card_resetAttributes(g, g.discard[i]);
    for (int i = 0; i < g.hand.size; ++i) card_resetAttributes(g, g.hand[i]);
    Effect end{};
    end.kind = EffectKind::RoomEndTurn;
    add_bottom(g, end);
}
SPIRE_FN inline void recharge_energy(Game& g) {
    int preserving_relic = -1;
    for (int i = 0; i < g.run.relics.size; ++i) {
        if (relic_data(g.run.relics[i].id).preserves_energy) {
            preserving_relic = i;
            break;
        }
    }
    if (preserving_relic >= 0) {
        if (g.player.energy > 0) queue_relic_above(g, preserving_relic, player_target, true);
        gain_energy(g, g.player.energy_per_turn);
    } else g.player.energy = g.player.energy_per_turn;
    g.manager.energy_gained_this_combat += g.player.energy_per_turn;
    for (int i = 0; i < g.run.relics.size; ++i) relic_onEnergyRecharge(g, i);
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_onEnergyRecharge(g, powers.at(g.power_objects, i), player_target);
}
SPIRE_FN inline void after_draw_turn_hooks(Game& g) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_atTurnStartPostDraw(g, i);
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_atStartOfTurnPostDraw(g, powers.at(g.power_objects, i), player_target);
}
SPIRE_FN inline void begin_player_turn(Game& g) {
    settle_card_moves(g);
    // AbstractRoom.endTurn and GameActionManager.getNextAction wait across the monster turn;
    // AbstractMonster.updateDeathAnimation has cleared deaths preceding that turn.
    for (int i = 0; i < g.monsters.size; ++i)
        if ((g.deaths_before_monster_turn >> i) & 1) g.monsters[i].creature.powers.clear();
    g.deaths_before_monster_turn = 0;
    if (!g.skip_monster_turn) {
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int id = g.monster_order[i];
            if (!g.monsters[id].creature.dying && !g.monsters[id].creature.escaped) at_end_of_turn(g, id);
        }
        at_end_of_round(g, player_target);
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int id = g.monster_order[i];
            if (!g.monsters[id].creature.dying && !g.monsters[id].creature.escaped) at_end_of_round(g, id);
        }
    }
    g.player.cards_played_this_turn = 0;
    g.orbs.turn_start = g.orbs.channeled.size;
    stance_start_turn(g);
    for (int i = 0; i < g.run.relics.size; ++i) relic_atTurnStart(g, i);
    for (int i = 0; i < g.hand.size; ++i) card_atTurnStartPreDraw(g, g.hand[i]);
    for (int i = 0; i < g.draw.size; ++i) card_atTurnStart(g, g.draw[i]);
    for (int i = 0; i < g.hand.size; ++i) card_atTurnStart(g, g.hand[i]);
    for (int i = 0; i < g.discard.size; ++i) card_atTurnStart(g, g.discard[i]);
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_atStartOfTurn(g, powers.at(g.power_objects, i), player_target);
    start_turn_orbs(g);
    ++g.turn;
    g.skip_monster_turn = false;
    g.manager.turn_has_ended = false;
    g.manager.total_discarded_this_turn = 0;
    g.manager.played_cards.begin_turn();
    g.manager.damage_received_this_turn = 0;
    if (!keeps_block(g, player_target))
        g.player.creature.block = max(0, g.player.creature.block - relic_block_loss(g));
    recharge_energy(g);
    for (int i = 0; i < g.monster_order.size; ++i) monster_createIntent(g, g.monster_order[i]);
    add_bottom(g, units::actions::Draw::construct(g.player.hand_size, true, -1, no_target, true));
    after_draw_turn_hooks(g);
    Effect enable{};
    enable.kind = EffectKind::EnableEndTurnButton;
    add_bottom(g, enable);
}
SPIRE_FN inline void complete_player_turn_effect(Game& g) {
    g.player_turn_effect_pending = false;
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_atEnergyGain(g, powers.at(g.power_objects, i), player_target);
}
SPIRE_FN inline void run_until_decision(Game& g) {
    if (g.player.escaping) {
        // AbstractPlayer.updateEscapeAnimation advances rule-read drawX at the oracle's fixed tick.
        while (g.player.escape_timer > 0.0f) {
            g.player.escape_timer -= 1.0f / 60.0f;
            const float speed = g.player.creature.flip_horizontal ? -400.0f : 500.0f;
            g.player.creature.draw_x += (1.0f / 60.0f) * speed * (1280.0f / 1920.0f);
        }
        g.battle_over = true;
        g.player.creature.flip_horizontal = false;
        g.player.escaping = false;
        g.player.escape_timer = 0.0f;
    }
    while (g.screen != Screen::Discard && g.screen != Screen::Grid && g.screen != Screen::CardReward && g.screen != Screen::Death) {
        bool dead = !g.monster_order.empty();
        for (int i = 0; i < g.monster_order.size; ++i) {
            const Creature& monster = g.monsters[g.monster_order[i]].creature;
            if (!monster.dead && !monster.escaped) dead = false;
        }
        if (dead && !g.cannot_lose) g.battle_over = true;
        if (g.manager.has_current_action) {
            execute_effect(g, g.action_objects[g.manager.current_action]);
            if (g.screen == Screen::Discard || g.screen == Screen::Grid || g.screen == Screen::CardReward || g.screen == Screen::Death) break;
            if (g.action_objects[g.manager.current_action].done) {
                if (!g.action_objects[g.manager.current_action].held) release_action(g, g.manager.current_action);
                g.manager.has_current_action = false;
            }
        } else if (!g.manager.actions.empty()) {
            if (g.manager.phase == Phase::ExecutingActions) g.manager.using_card = false;
            g.manager.phase = Phase::ExecutingActions;
            g.manager.current_action = g.manager.actions.pop_front();
            g.manager.has_current_action = true;
            g.manager.has_control = true;
        } else if (!g.manager.pre_turn_actions.empty()) {
            if (g.manager.phase == Phase::ExecutingActions) g.manager.using_card = false;
            g.manager.phase = Phase::ExecutingActions;
            g.manager.current_action = g.manager.pre_turn_actions.pop_front();
            g.manager.has_current_action = true;
            g.manager.has_control = true;
        } else if (!g.manager.card_queue.empty()) {
            g.manager.using_card = g.manager.phase == Phase::WaitingOnUser;
            CardQueueItem queued = g.manager.card_queue[0];
            if (g.manager.card_queue.size == 1 && queued.is_end_turn_auto_play)
                for (int i = 0; i < g.run.relics.size; ++i) relic_disableUntilTurnEnds(g, i);
            if (queued.card < 0) {
                begin_end_turn(g);
            } else {
                Card& card = g.cards[queued.card];
                card.in_autoplay = queued.autoplay_card;
                if (queued.random_target) {
                    queued.target = random_living_monster(g);
                    g.manager.card_queue[0].target = queued.target;
                }
                if (!card_canUse(g, queued.card, queued.target) && !card.dont_trigger_on_use_card) {
                    remove_from_pile(g.limbo, queued.card);
                    g.manager.card_queue.pop_front();
                    if (card.in_autoplay) {
                        card.dont_trigger_on_use_card = true;
                        Effect use = units::actions::UseCard::construct(g, queued.card, no_target);
                        add_bottom(g, use);
                    }
                    continue;
                }
                if (card_freeToPlay(g, queued.card)) card.free_to_play_once = true;
                card.energy_on_use = queued.energy_on_use;
                card.ignore_energy_on_use = card.in_autoplay || queued.ignore_energy_total;
                if (!card.dont_trigger_on_use_card) {
                    play_card_hooks(g, queued.card, queued.target);
                    ++g.player.cards_played_this_turn;
                    g.manager.played_cards.record(queued.card);
                }
                if (card.target != CardTarget::Enemy ||
                    (queued.target != no_target && !get_creature(g, queued.target).dead && !get_creature(g, queued.target).escaped))
                    use_card(g, queued.card, queued.target);
                else remove_from_pile(g.limbo, queued.card);
            }
            remove_queued_card(g, queued.card);
        } else if (g.player.end_turn_queued && !monsters_basically_dead(g)) {
            // AbstractRoom.update skips AbstractPlayer.updateInput after the final monster dies.
            complete_end_turn(g);
        } else if (!g.manager.monster_attacks_queued) {
            g.manager.monster_attacks_queued = true;
            g.deaths_before_monster_turn = 0;
            for (int i = 0; i < g.monsters.size; ++i)
                if (g.monsters[i].creature.dead) g.deaths_before_monster_turn |= uint64_t{1} << i;
            for (int i = 0; i < g.monster_order.size; ++i) {
                const int id = g.monster_order[i];
                const Creature& c = g.monsters[id].creature;
                if (!g.skip_monster_turn && (alive(c) || c.half_dead)) g.manager.monster_queue.push_back(id);
            }
            if (g.manager.phase == Phase::ExecutingActions && !g.manager.using_card) {
                g.manager.phase = Phase::WaitingOnUser;
                g.manager.has_control = false;
                refresh_hand(g);
            }
            g.manager.using_card = false;
        } else if (!g.manager.monster_queue.empty()) {
            const int id = g.manager.monster_queue.pop_front();
            if (alive(g.monsters[id].creature) || g.monsters[id].creature.half_dead) {
                for (int i = 0; i < g.monsters.size; ++i)
                    if (g.monsters[i].creature.dead) g.deaths_before_monster_turn |= uint64_t{1} << i;
                if (g.monsters[id].intent != Intent::None) {
                    Effect show{};
                    show.kind = EffectKind::ShowMoveName;
                    action_initialize_state(show);
                    show.source = id;
                    show.target = id;
                    add_bottom(g, show);
                    add_bottom(g, units::actions::IntentFlash::make(id));
                }
                monster_take_turn(g, id);
                Powers& powers = g.monsters[id].creature.powers;
                for (int i = 0; i < powers.size; ++i) power_duringTurn(g, powers.at(g.power_objects, i), id);
            }
            // GameActionManager.update refreshes the hand when getNextAction queued a monster turn but selected no action.
            if (g.manager.phase == Phase::ExecutingActions && !g.manager.using_card) {
                g.manager.phase = Phase::WaitingOnUser;
                g.manager.has_control = false;
                refresh_hand(g);
            }
            g.manager.using_card = false;
        } else if (g.battle_over) {
            on_victory(g);
            clear_actions(g, g.manager.actions);
            clear_actions(g, g.manager.pre_turn_actions);
            g.manager.has_current_action = false;
            g.manager.card_queue.clear();
            g.manager.played_cards.clear();
            g.orbs.channeled.clear(); g.orbs.turn_start = 0;
            for (auto& count : g.stance_changes) count = 0;
            g.manager.energy_gained_this_combat = 0;
            g.manager.mantra_gained = 0;
            g.manager.damage_received_this_combat = 0;
            g.manager.damage_received_this_turn = 0;
            g.manager.hp_loss_this_combat = 0;
            g.manager.turn_has_ended = false;
            g.manager.total_discarded_this_turn = 0;
            g.turn = 1;
            g.room_in_combat = false;
            g.room_phase = RoomPhase::Complete;
            combat_rewards(g);
            if (g.room_kind == RoomKind::Event && !g.room_reward_allowed) event_reopen(g);
            g.manager.phase = Phase::WaitingOnUser;
            g.manager.has_control = false;
            g.manager.using_card = false;
            break;
        } else if (g.manager.turn_has_ended && !monsters_basically_dead(g)) {
            begin_player_turn(g);
        } else {
            if (g.player_turn_effect_pending) {
                complete_player_turn_effect(g);
                if (!g.manager.actions.empty()) continue;
            }
            if (g.room_in_combat) {
                g.manager.phase = Phase::WaitingOnUser;
                g.manager.has_control = false;
            }
            g.manager.using_card = false;
            refresh_hand(g);
            if (!g.manager.actions.empty()) {
                g.manager.phase = Phase::ExecutingActions;
                continue;
            }
            break;
        }
    }
    settle_card_obtains(g);
    settle_card_moves(g);
    for (int i = 0; i < g.monster_order.size; ++i) {
        Creature& creature = g.monsters[g.monster_order[i]].creature;
        if (creature.dead) {
            creature.powers.clear();
        }
    }
}
SPIRE_FN inline void construct_encounter(Game& g, EncounterId encounter) {
    const EncounterId previous = g.encounter;
    g.encounter = encounter;
    g.prepared_encounter = encounter;
    g.room_has_monsters = true;
    encounter_initialize_state(g);
    g.power_objects.clear(); g.next_power_instance = 1;
    g.move_histories.size = 0;
    g.monsters.clear(); g.monster_damage_extra.clear(); g.monster_order.clear();
    build_encounter(g, encounter);
    g.encounter = previous;
}
SPIRE_FN inline void prepare_player_battle(Game& g) {
    g.manager.player_hp_last_turn = g.player.creature.hp;
    g.player.bloodied = g.player.creature.hp <= g.player.creature.max_hp / 2;
    g.turn = 1;
    g.battle_over = false;
    g.room_in_combat = true;
    g.room_phase = RoomPhase::Combat;
    g.room_reward_time = false;
    g.cannot_lose = false;
    g.mugged = false;
    g.smoked = false;
    g.discard_remaining = 0;
    g.selection_any_number = g.selection_can_pick_zero = false;
    g.end_turn_enabled = true;
    g.screen = Screen::Combat;
    g.player.hand_size = g.player.master_hand_size;
    g.player.energy = 0;
    g.skip_monster_turn = false;
    g.player.cards_played_this_turn = 0;
    g.orbs.turn_start = g.orbs.channeled.size;
    g.player.end_turn_queued = false;
    g.player.card_in_use = -1;
    g.player.max_orbs = g.player.master_max_orbs;
    g.orbs = OrbState{};
    for (int i = 0; i < g.player.max_orbs; ++i) g.orbs.slots.push_back(-1);
    g.player.stance = Stance::Neutral;
    for (auto& count : g.stance_changes) count = 0;
    Pile copy{};
    for (int i = 0; i < g.run.master_deck.size; ++i) {
        const int index = g.cards.size;
        g.cards.push_back(g.run.master_deck[i]);
        g.cards[index].energy_on_use = 0; g.cards[index].priced = false;
        copy.push_back(index);
    }
    shuffle(copy.values, copy.size, g.rng.shuffle);
    Pile innate{};
    for (int i = 0; i < copy.size; ++i) {
        const int card = copy[i];
        if (g.cards[card].innate || g.cards[card].bottle_flame || g.cards[card].bottle_lightning || g.cards[card].bottle_tornado) innate.push_back(card);
        else g.draw.push_back(card);
    }
    for (int i = 0; i < innate.size; ++i) g.draw.push_back(innate[i]);
    if (innate.size > g.player.hand_size) {
        g.manager.pre_turn_actions.push_front(g.action_objects.add(units::actions::Draw::construct(innate.size - g.player.hand_size)));
    }
    for (int i = 0; i < g.run.relics.size; ++i) relic_beforeEnergyPrep(g, i);
    g.player.energy_per_turn = g.player.energy_master;
    g.player.creature.powers.clear();
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int owner = g.monster_order[i];
        monster_pre_battle(g, owner);
        monster_useUniversalPreBattleAction(g, owner);
    }
    if (g.room_kind == RoomKind::Elite && g.run.map[g.run.node_y][g.run.node_x].emerald) {
        const int buff = g.rng.map.random(0, 3);
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int owner = g.monster_order[i];
            if (buff == 1) add_bottom(g, units::actions::IncreaseMaxHp::construct(owner, 0.25f, true));
            else {
                const PowerId power = buff == 0 ? PowerId::Strength : buff == 2 ? PowerId::Metallicize : PowerId::RegenerateMonster;
                const int amount = buff == 0 ? g.run.act + 1 : buff == 2 ? g.run.act * 2 + 2 : g.run.act * 2 + 1;
                queue_power(g, owner, owner, power, amount);
            }
        }
    }
    for (int i = 0; i < g.run.relics.size; ++i) relic_atPreBattle(g, i);
}
SPIRE_FN inline void enter_combat(Game& g) {
    Card retained[64]{};
    for (int i = 0; i < g.detached_master_cards.size; ++i) retained[i] = g.cards[g.detached_master_cards[i]];
    g.cards.clear();
    for (auto& word : g.pending_card_moves) word = 0;
    g.foreign_reward_cards.clear();
    for (int i = 0; i < g.detached_master_cards.size; ++i) { g.detached_master_cards[i] = i; g.cards.push_back(retained[i]); }
    g.previous_screen = Screen::None;
    g.multi_damage.clear();
    g.damage_exceptions.clear();
    g.hand.clear(); g.draw.clear(); g.discard.clear(); g.exhaust.clear(); g.limbo.clear();
    g.selected_cards.clear();
    g.limbo.clear(); g.grid_cards.clear(); g.reward_cards.clear();
    g.grid_source = PileId::Grid;
    g.discovery_card = -1;
    g.action_objects.clear();
    g.player_turn_effect_pending = false;
    g.selection_for_upgrade = false;
    g.drawn_cards.clear();
    g.selection_any_number = false;
    g.selection_can_pick_zero = false;
    g.manager = ActionManager{};
    g.encounter = g.prepared_encounter;
    g.combat_metric_event = EventId::Count;
    if (g.event.id != EventId::Count) {
        g.event.has_focus = false;
        if (!event_data(g.event.id).image) g.room_dialog.clear();
    }
    for (int i = 0; i < g.monster_order.size; ++i) roll_monster_move(g, g.monster_order[i]);
    prepare_player_battle(g);
    run_until_decision(g);
    g.manager.turn_has_ended = true;
    g.end_turn_enabled = false;
    add_bottom(g, units::actions::GainEnergyAndEnableControls::make(g.player.energy_master));
    for (int i = 0; i < g.run.relics.size; ++i) relic_atBattleStartPreDraw(g, i);
    Effect draw{};
    draw.kind = EffectKind::Draw;
    draw.amount = g.player.hand_size;
    add_bottom(g, draw);
    Effect enable{};
    enable.kind = EffectKind::EnableEndTurnButton;
    add_bottom(g, enable);
    relics_at_battle_start(g);
    stance_start_turn(g);
    for (int i = 0; i < g.run.relics.size; ++i) relic_atTurnStart(g, i);
    for (int i = 0; i < g.run.relics.size; ++i) relic_atTurnStartPostDraw(g, i);
    for (int i = 0; i < g.draw.size; ++i) card_atTurnStart(g, g.draw[i]);
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_atStartOfTurn(g, powers.at(g.power_objects, i), player_target);
    start_turn_orbs(g);
    run_until_decision(g);
    for (int i = 0; i < g.monster_order.size; ++i) monster_createIntent(g, g.monster_order[i]);
    refresh_hand(g);
}
SPIRE_FN inline void initialize_source_card_pools(Game& g) {
    const int pool_sizes[] = {
        static_cast<int>(sizeof(common_card_pool) / sizeof(*common_card_pool)),
        static_cast<int>(sizeof(uncommon_card_pool) / sizeof(*uncommon_card_pool)),
        static_cast<int>(sizeof(rare_card_pool) / sizeof(*rare_card_pool)),
        static_cast<int>(sizeof(colorless_card_pool) / sizeof(*colorless_card_pool))};
    g.run.colorless_pool.clear();
    for (int i = 0; i < pool_sizes[3]; ++i) g.run.colorless_pool.push_back(i);
    // AbstractDungeon.initializeCardPools copies each pool to its source pool with addToBottom.
    for (int p = 0; p < 4; ++p) {
        g.run.source_card_pools[p].clear();
        for (int i = pool_sizes[p] - 1; i >= 0; --i) g.run.source_card_pools[p].push_back(i);
    }
}
constexpr Game initial_game{};

SPIRE_FN inline void reset(Game& g, uint64_t seed, RunProfile profile = {}) {
    bind_fault(g);
    g = initial_game;
    g.run.seed = seed;
    g.run.profile = profile;
    g.unseeded.reset(seed);
    g.rng.generate_seeds(seed);
    g.rng.reseed_map(seed, 1);
    g.player.creature.max_hp = 66;
    g.player.creature.hp = 59;
    g.player.energy_per_turn = 0;
    starter_deck(g.run);
    initialize_source_card_pools(g);
    initialize_dungeon(g);
    for (int i = 0; i < g.run.master_deck.size; ++i) g.run.master_deck[i].uuid = g.next_card_uuid++;
    g.player.creature.draw_x = 320.0f;
    g.turn = 0;
    g.encounter = EncounterId::Count;
    g.room_kind = RoomKind::Neow;
    g.event.id = EventId::Neow;
    event_initialize_state(g.event);
    event_enter(g);
}
SPIRE_FN inline void enter_room(Game& g, RoomKind room) {
    g.orbs.slots.clear();
    g.hand.clear(); g.draw.clear(); g.discard.clear(); g.exhaust.clear(); g.limbo.clear();
    g.player.creature.powers.clear();
    g.power_objects.clear(); g.next_power_instance = 1;
    g.player.creature.block = 0;
    g.turn = 1;
    g.room_rewards.clear(); g.screen_rewards.clear(); g.reward_objects.clear();
    g.reward_cards.clear();
    g.previous_screen = Screen::None;
    g.acquisition_reward = -1;
    g.rng.reseed_floor(g.run.seed, g.run.floor);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onEnterRoom(g, i, static_cast<int>(room));
    g.room_combat_event = false;
    if (room == RoomKind::Event) {
        room = roll_event_room(g);
        g.run.map[g.run.node_y][g.run.node_x].room = room;
        g.room_combat_event = room == RoomKind::Monster || room == RoomKind::Elite;
    }
    g.room_kind = room;
    g.room_elite_trigger = room == RoomKind::Elite;
    g.room_in_combat = false;
    g.room_phase = RoomPhase::Incomplete;
    g.room_reward_time = false;
    g.room_reward_allowed = true;
    g.room_has_monsters = false;
    g.room_rare_card_chance = 3;
    g.room_uncommon_card_chance = 37;
    g.battle_over = false;
    g.smoked = g.mugged = g.cannot_lose = false;
    if (g.event.id != EventId::Count) { g.room_dialog.clear(); g.image_dialog.clear(); }
    g.event = Event{};
    g.text.clear();
    g.room_event_class = 0;
    // DiscardAction.update retains HAND_SELECT cards; Beggar.update reads a separate GRID list.
    g.selected_cards.clear();
    g.monster_order.clear();
    for (int i = 0; i < g.run.relics.size; ++i) relic_justEnteredRoom(g, i, static_cast<int>(room));
    g.player.creature.draw_x = room == RoomKind::Elite && g.run.act == 4 ? 640.0f : 320.0f;
    g.player.creature.flip_horizontal = false;
    switch (room) {
    case RoomKind::Monster: construct_encounter(g, g.run.monster_list[0]); enter_combat(g); break;
    case RoomKind::Elite: construct_encounter(g, g.run.elite_list[0]); enter_combat(g); break;
    case RoomKind::Boss:
        g.run.boss_list.erase(0);
        construct_encounter(g, g.run.boss_key); enter_combat(g);
        break;
    case RoomKind::Rest: enter_campfire(g); break;
    case RoomKind::Treasure: enter_treasure(g); break;
    case RoomKind::BossTreasure: enter_boss_chest(g); break;
    case RoomKind::Shop: enter_shop(g); break;
    case RoomKind::Event: enter_event(g); break;
    case RoomKind::HeartEvent:
        g.event.id = EventId::SpireHeart; event_initialize_state(g.event);
        g.room_event_class = g.text.intern(event_data(g.event.id).java_class);
        g.screen = Screen::Event; g.room_phase = RoomPhase::Event;
        event_enter(g);
        break;
    case RoomKind::Victory:
        g.player.creature.draw_x = 640.0f;
        g.screen = Screen::Victory;
        break;
    default: SPIRE_ASSERT(false);
    }
}
SPIRE_FN inline void start_combat(Game& g, const CombatSetup& setup) {
    if (g.fault != 0) return;
    bind_fault(g);
    g.run.ruby_key = setup.keys[0]; g.run.emerald_key = setup.keys[1]; g.run.sapphire_key = setup.keys[2];
    if (setup.act != 1) {
        g.run.act = setup.act;
        if (g.rng.card.counter > 0 && g.rng.card.counter < 750) g.rng.card.setCounter(((g.rng.card.counter - 1) / 250 + 1) * 250);
        initialize_dungeon(g);
    }
    g.run.floor = setup.first_room_chosen ? setup.floor - 1 : 0;
    g.room_dialog.clear(); g.image_dialog.clear();
    g.event = Event{};
    g.run.gold = setup.gold;
    if (setup.room == RoomKind::Elite) g.run.elite_list[0] = setup.encounter;
    else if (setup.room == RoomKind::Boss) g.run.boss_key = setup.encounter;
    else if (setup.room == RoomKind::Monster) g.run.monster_list[0] = setup.encounter;
    g.run.master_deck = setup.deck;
    g.cards.clear();
    g.detached_master_cards.clear();
    g.run.relics.clear();
    g.run.potion_slots = 2;
    g.room_in_combat = false;
    g.player = Player{};
    g.player.creature.hp = 59;
    g.player.creature.max_hp = 66;
    g.next_card_uuid = 1;
    for (int i = 0; i < g.run.master_deck.size; ++i) g.run.master_deck[i].uuid = g.next_card_uuid++;
    RandomXS128 acquisition(g.run.seed);
    g.screen = Screen::None;
    for (int i = 0; i < setup.relics.size; ++i) {
        equip_relic(g, setup.relics[i].id);
        settle_relic_acquisition(g, acquisition);
    }
    for (int i = 0; i < g.run.potion_slots; ++i) {
        g.run.potions[i] = setup.potions[i];
        potion_initialize_state(g, i);
        g.run.potion_state[i] = construct_potion(g, setup.potions[i]);
    }
    g.room_rewards.clear(); g.screen_rewards.clear(); g.reward_objects.clear();
    if (setup.room == RoomKind::Neow) {
        g.run.floor = setup.floor;
        g.player.creature.hp = setup.hp; g.player.creature.max_hp = setup.max_hp;
        g.run.node_x = 0; g.run.node_y = -1;
        g.player.creature.draw_x = 320.0f;
        g.player.energy_per_turn = 0;
        g.room_kind = RoomKind::Neow;
        g.event.id = EventId::Neow; event_initialize_state(g.event); event_enter(g);
        return;
    }
    g.run.floor = setup.floor;
    g.player.energy_per_turn = 0;
    g.rng.reseed_floor(g.run.seed, setup.floor);
    g.player.creature.draw_x = 1280.0f * 0.25f;
    g.player.creature.hp = setup.hp;
    g.player.creature.max_hp = setup.max_hp;
    g.room_kind = RoomKind::Monster;
    g.run.node_x = g.run.next_x = setup.node_x;
    g.run.node_y = g.run.next_y = setup.node_y;
    g.run.node_on_map = setup.first_room_chosen;
    g.run.first_room_chosen = setup.first_room_chosen;
    if (setup.first_room_chosen) {
        g.run.path_x.push_back(setup.node_x); g.run.path_y.push_back(setup.node_y);
        auto& node = g.run.map[setup.node_y][setup.node_x];
        node.room = setup.room; node.emerald = setup.emerald;
    }
    enter_room(g, setup.room);
}
SPIRE_FN inline void open_map(Game& g) {
    g.previous_screen = g.screen == Screen::None && g.event.id != EventId::Count ? Screen::Event : g.screen;
    g.run.map_dismissable = true;
    g.screen = Screen::Map;
    g.room_phase = RoomPhase::Complete;
}
SPIRE_FN inline void update_event(Game& g) {
    if (g.event.id != EventId::Count) {
        event_update(g);
        if (event_data(g.event.id).image && !g.room_in_combat && (g.screen == Screen::None || g.screen == Screen::Event))
            g.event.has_focus = true;
    }
    settle_card_obtains(g);
}
SPIRE_FN inline void complete_master_selection(Game& g) {
    // AbstractDungeon.closeCurrentScreen reopens a reward screen saved by BottledLightning.onEquip.
    g.screen = g.previous_screen == Screen::CombatReward ? Screen::CombatReward : Screen::None;
    for (int i = 0; i < g.run.relics.size; ++i) relic_update(g, i);
    if (g.room_kind == RoomKind::Rest) finish_campfire_selection(g);
    if (g.room_kind == RoomKind::BossTreasure && g.screen == Screen::None) g.screen = Screen::Room;
    if (g.room_kind == RoomKind::Shop) {
        if (g.grid_for_purge) finish_shop_purge(g);
        g.screen = Screen::Shop;
        g.previous_screen = g.room_reward_time ? Screen::CombatReward : Screen::None;
    }
    update_event(g);
    if (g.event.id != EventId::Count && g.screen == Screen::None) g.screen = Screen::Event;
    settle_card_obtains(g);
    g.grid_source = PileId::Grid;
    g.grid_for_purge = g.grid_for_upgrade = g.grid_for_transform = false;
    g.grid_can_cancel = true;
}
SPIRE_FN inline void map_choices(const Game& g, List<uint8_t, 8>& choices) {
    choices.clear();
    if (g.run.node_y == 14 || (g.run.act == 4 && g.run.node_y == 2)) { choices.push_back(105); return; }
    bool winged = false;
    for (int i = 0; i < g.run.relics.size; ++i)
        winged |= relic_data(g.run.relics[i].id).crosses_map_paths && g.run.relics[i].counter > 0;
    const int y = g.run.first_room_chosen ? g.run.node_y + 1 : 0;
    for (int x = 0; x < 7; ++x) {
        if (!g.run.map[y][x].edges) continue;
        if (!g.run.first_room_chosen || (g.run.map[g.run.node_y][g.run.node_x].edges & (1 << x)) || (winged && g.run.map[g.run.node_y][g.run.node_x].edges))
            choices.push_back(y * 7 + x);
    }
}
SPIRE_FN inline void enter_map_node(Game& g, int index) {
    if (index != 105 && g.run.first_room_chosen && !(g.run.map[g.run.node_y][g.run.node_x].edges & (1 << (index % 7))))
        for (int i = 0; i < g.run.relics.size; ++i) if (relic_data(g.run.relics[i].id).crosses_map_paths) {
            if (--g.run.relics[i].counter <= 0) relic_setCounter(g, i, -2);
            break;
        }
    // Closing a nondismissable map through DungeonMap's boss button invokes firstRoomLogic.
    if (index == 105 && !g.run.first_room_chosen && !g.run.map_dismissable && g.room_kind != RoomKind::Monster && g.room_kind != RoomKind::Elite && g.room_kind != RoomKind::Boss) ++g.run.floor;
    if (g.run.node_on_map) g.run.map[g.run.node_y][g.run.node_x].taken = true;
    const int x = index == 105 ? -1 : index % 7;
    const int y = index == 105 ? 15 : index / 7;
    const RoomKind room = index == 105 ? RoomKind::Boss : g.run.map[y][x].room;
    if (g.room_kind == RoomKind::Elite) g.run.elite_list.erase(0);
    else if (g.room_kind == RoomKind::Monster || g.room_kind == RoomKind::Boss) g.run.monster_list.erase(0);
    g.run.next_x = g.run.node_x = x;
    g.run.next_y = g.run.node_y = y;
    g.run.first_room_chosen = true;
    g.run.node_on_map = index != 105;
    if (index == 105) {
        const bool has_path = g.run.path_y.size > 1;
        g.run.path_x.push_back(has_path ? g.run.path_x[g.run.path_x.size - 1] : 1);
        g.run.path_y.push_back(has_path ? g.run.path_y[g.run.path_y.size - 1] + 1 : 15);
    } else { g.run.path_x.push_back(x); g.run.path_y.push_back(y); }
    ++g.run.floor;
    enter_room(g, room);
}
SPIRE_FN inline void next_act(Game& g) {
    ++g.run.act;
    if (g.rng.card.counter > 0 && g.rng.card.counter < 750)
        g.rng.card.setCounter(((g.rng.card.counter - 1) / 250 + 1) * 250);
    g.run.path_x.clear(); g.run.path_y.clear();
    g.run.event_chances[0] = 0.0f; g.run.event_chances[1] = 0.1f;
    g.run.event_chances[2] = 0.03f; g.run.event_chances[3] = 0.02f;
    g.run.potion_blizz_modifier = 0;
    heal_creature(g, player_target, floor_int((g.player.creature.max_hp - g.player.creature.hp) * 0.75f + 0.5f));
    ActionManager manager{};
    manager.total_discarded_this_turn = g.manager.total_discarded_this_turn;
    manager.damage_received_this_turn = g.manager.damage_received_this_turn;
    manager.damage_received_this_combat = g.manager.damage_received_this_combat;
    manager.hp_loss_this_combat = g.manager.hp_loss_this_combat;
    manager.player_hp_last_turn = g.manager.player_hp_last_turn;
    manager.energy_gained_this_combat = g.manager.energy_gained_this_combat;
    g.manager = manager;
    initialize_source_card_pools(g);
    initialize_dungeon(g);
    g.run.first_room_chosen = false;
    g.run.node_on_map = false;
    g.run.map_dismissable = false;
    g.room_phase = RoomPhase::Complete;
    if (g.run.act == 2) {
        g.run.node_x = 0; g.run.node_y = -1;
        g.room_kind = RoomKind::Empty;
        g.room_in_combat = g.room_reward_time = g.battle_over = false;
        g.room_elite_trigger = g.room_combat_event = g.smoked = g.mugged = g.cannot_lose = false;
        g.room_rare_card_chance = 3; g.room_uncommon_card_chance = 37;
        g.room_rewards.clear(); g.screen_rewards.clear();
        g.monster_order.clear();
        g.event = Event{}; g.room_event_class = 0;
    }
    g.screen = Screen::Map;
}
SPIRE_FN inline void enter_boss_treasure(Game& g) {
    g.run.monster_list.erase(0);
    g.run.next_x = g.run.node_x = -1;
    g.run.next_y = g.run.node_y = 15;
    ++g.run.floor;
    enter_room(g, RoomKind::BossTreasure);
}
SPIRE_FN inline void legal_actions(const Game& g, ActionSet& actions) {
    actions.clear();
    if (g.fault != 0) return;
    if (g.screen == Screen::Death || g.screen == Screen::Victory || g.screen == Screen::None) return;
    if (g.screen == Screen::Combat) {
        for (int i = 0; i < g.hand.size; ++i) {
            const Card& card = g.cards[g.hand[i]];
            if (card.target == CardTarget::Enemy || card.target == CardTarget::SelfAndEnemy) {
                for (int j = 0; j < g.monster_order.size; ++j) {
                    if (alive(g.monsters[g.monster_order[j]].creature) && card_canUse(g, g.hand[i], g.monster_order[j])) actions.push_back({ActionKind::Play, i, j});
                }
            } else if (card_canUse(g, g.hand[i], no_target)) actions.push_back({ActionKind::Play, i, -1});
        }
        if (g.end_turn_enabled) actions.push_back({ActionKind::EndTurn, -1, -1});
    }
    for (int slot = 0; slot < g.run.potion_slots; ++slot) {
        const PotionData data = potion_data(g.run.potions[slot]);
        if (!data.usable) continue;
        if (potion_canUse(g, slot)) {
            if (data.target == CardTarget::Enemy) {
                for (int i = 0; i < g.monster_order.size; ++i)
                    if (alive(g.monsters[g.monster_order[i]].creature)) actions.push_back({ActionKind::PotionUse, slot, i});
            } else actions.push_back({ActionKind::PotionUse, slot, -1});
        }
        if (potion_canDiscard(g, slot)) actions.push_back({ActionKind::PotionDiscard, slot, -1});
    }
    if (g.screen == Screen::Room) {
        if (g.room_kind == RoomKind::Rest && !g.campfire.something_selected) {
            int option = 0;
            for (int i = 0; i < g.campfire.options.size; ++i)
                if (g.campfire.options[i].usable) actions.push_back({ActionKind::Choose, option++, -1});
        }
        if ((g.room_kind == RoomKind::Treasure || g.room_kind == RoomKind::BossTreasure) && !g.chest.open) actions.push_back({ActionKind::Choose, 0, -1});
        if (g.room_kind == RoomKind::Shop) actions.push_back({ActionKind::Choose, 0, -1});
        if (g.room_phase == RoomPhase::Complete) actions.push_back({ActionKind::Proceed, -1, -1});
    } else if (g.screen == Screen::BossReward) {
        for (int i = 0; i < g.chest.relics.size; ++i) actions.push_back({ActionKind::Choose, i, -1});
        actions.push_back({ActionKind::Skip, -1, -1});
    } else if (g.screen == Screen::Event) {
        EventChoices choices{}; event_choices(g, choices);
        for (int i = 0; i < choices.size; ++i) actions.push_back({ActionKind::Choose, i, -1});
    } else if (g.screen == Screen::Shop) {
        List<uint8_t, 14> choices{}; shop_choices(g, choices);
        for (int i = 0; i < choices.size; ++i) actions.push_back({ActionKind::Choose, i, -1});
        actions.push_back({ActionKind::Skip, -1, -1});
    } else if (g.screen == Screen::Map) {
        List<uint8_t, 8> choices{}; map_choices(g, choices);
        for (int i = 0; i < choices.size; ++i) actions.push_back({ActionKind::Choose, i, -1});
        if (g.run.map_dismissable) actions.push_back({ActionKind::Skip, -1, -1});
    } else if (g.screen == Screen::Discard) {
        if (g.selected_cards.size < g.selection_amount)
            for (int i = 0; i < g.hand.size; ++i) actions.push_back({ActionKind::Choose, i, -1});
        if ((g.selection_can_pick_zero && g.selected_cards.empty()) ||
            ((!g.selected_cards.empty() || g.selection_can_pick_zero) &&
                (g.selection_any_number || g.selection_up_to || g.selected_cards.size == g.selection_amount)))
            actions.push_back({ActionKind::Proceed, -1, -1});
    } else if (g.screen == Screen::CardReward) {
        for (int i = 0; i < g.reward_cards.size; ++i) actions.push_back({ActionKind::Choose, i, -1});
        if (!g.room_in_combat)
            for (int i = 0; i < g.run.relics.size; ++i) if (relic_data(g.run.relics[i].id).bowl_hp) {
                actions.push_back({ActionKind::Choose, g.reward_cards.size, -1}); break;
            }
        if (g.reward_skippable) actions.push_back({ActionKind::Skip, -1, -1});
    } else if (g.screen == Screen::Grid) {
        if (g.selection_confirmation || g.grid_confirm_screen) {
            actions.push_back({ActionKind::Proceed, -1, -1});
            if (g.grid_confirm_screen) actions.push_back({ActionKind::Skip, -1, -1});
            return;
        }
        for (int i = 0; i < get_pile(g, g.grid_source).size; ++i) actions.push_back({ActionKind::Choose, i, -1});
        if (g.selection_any_number) actions.push_back({ActionKind::Proceed, -1, -1});
        if (!g.room_in_combat && g.grid_can_cancel &&
            (g.grid_for_upgrade || g.grid_for_transform || g.grid_for_purge || g.room_kind == RoomKind::Shop))
            actions.push_back({ActionKind::Skip, -1, -1});
    } else if (g.screen == Screen::CombatReward) {
        for (int i = 0; i < g.screen_rewards.size; ++i) actions.push_back({ActionKind::Choose, i, -1});
        if (g.reward_proceed_visible) actions.push_back({ActionKind::Proceed, -1, -1});
        if (g.combat_reward_skippable) actions.push_back({ActionKind::Skip, -1, -1});
    }
}
SPIRE_FN inline void step(Game& g, Action action) {
    if (g.fault != 0) return;
    bind_fault(g);
    if (action.kind == ActionKind::PotionUse || action.kind == ActionKind::PotionDiscard) {
        SPIRE_ASSERT(potion_data(g.run.potions[action.index]).usable);
        SPIRE_ASSERT(action.kind != ActionKind::PotionDiscard || potion_canDiscard(g, action.index));
        if (action.kind == ActionKind::PotionUse) {
            SPIRE_ASSERT(potion_canUse(g, action.index));
            const int target = action.target >= 0 ? g.monster_order[action.target] : player_target;
            potion_use(g, action.index, target);
            for (int i = 0; i < g.run.relics.size; ++i) relic_onUsePotion(g, i);
        }
        g.run.potions[action.index] = PotionId::Empty;
        g.run.potion_identity[action.index] = 0;
        if (g.screen == Screen::Combat) run_until_decision(g);
        return;
    }
    if (g.screen == Screen::Room) {
        if (action.kind == ActionKind::Proceed) {
            if (g.room_kind == RoomKind::Boss && g.run.act == 4) {
                g.run.monster_list.erase(0);
                g.run.next_x = g.run.node_x = 3;
                g.run.next_y = g.run.node_y = 4;
                ++g.run.floor;
                enter_room(g, RoomKind::Victory);
            } else if (g.room_kind == RoomKind::Boss && g.run.act == 3) {
                const bool double_boss = g.run.boss_list.size == 2;
                if (double_boss) g.run.boss_key = g.run.boss_list[0];
                g.run.monster_list.erase(0);
                g.run.next_x = g.run.node_x = -1;
                g.run.next_y = g.run.node_y = 15;
                ++g.run.floor;
                enter_room(g, double_boss ? RoomKind::Boss : RoomKind::HeartEvent);
            } else if (g.room_kind == RoomKind::BossTreasure) next_act(g);
            else open_map(g);
        }
        else if (g.room_kind == RoomKind::Shop) { g.screen = Screen::Shop; }
        else if (g.room_kind == RoomKind::BossTreasure) open_boss_chest(g);
        else if (g.room_kind == RoomKind::Treasure) {
            SPIRE_ASSERT(action.kind == ActionKind::Choose && action.index == 0);
            open_chest(g);
            run_until_decision(g);
        } else {
            SPIRE_ASSERT(action.kind == ActionKind::Choose && g.room_kind == RoomKind::Rest);
            int option = action.index;
            for (int i = 0; i < g.campfire.options.size; ++i)
                if (g.campfire.options[i].usable && option-- == 0) { choose_campfire(g, i); settle_card_obtains(g); return; }
            SPIRE_ASSERT(false);
        }
        return;
    }
    if (g.screen == Screen::BossReward) {
        if (action.kind == ActionKind::Skip) { g.chest.open = false; g.screen = Screen::Room; }
        else { SPIRE_ASSERT(action.kind == ActionKind::Choose); choose_boss_relic(g, action.index); settle_card_obtains(g); }
        return;
    }
    if (g.screen == Screen::Event) {
        SPIRE_ASSERT(action.kind == ActionKind::Choose);
        EventChoices choices{}; event_choices(g, choices);
        const EventChoice choice = choices[action.index];
        if (choice.kind == EventChoiceKind::Option) {
            if (event_data(g.event.id).image) g.image_selected_option = choice.index;
            else g.room_selected_option = choice.index;
        }
        event_choose(g, choice.index);
        update_event(g);
        return;
    }
    if (g.screen == Screen::Shop) {
        if (action.kind == ActionKind::Skip) g.screen = Screen::Room;
        else {
            SPIRE_ASSERT(action.kind == ActionKind::Choose);
            List<uint8_t, 14> choices{}; shop_choices(g, choices);
            purchase_shop(g, choices[action.index]);
        }
        return;
    }
    if (g.screen == Screen::Map) {
        if (action.kind == ActionKind::Skip) g.screen = g.previous_screen;
        else {
            SPIRE_ASSERT(action.kind == ActionKind::Choose);
            List<uint8_t, 8> choices{}; map_choices(g, choices);
            enter_map_node(g, choices[action.index]);
        }
        return;
    }
    if (!g.room_in_combat && g.screen == Screen::CombatReward) {
        if (action.kind == ActionKind::Choose) {
            RoomReward& reward = g.reward_objects[g.screen_rewards[action.index]];
            if (reward.kind == RewardKind::Gold || reward.kind == RewardKind::StolenGold) {
                gain_gold(g, reward.amount + reward.bonus_gold);
                reward.done = true;
                g.screen_rewards.erase(action.index);
                return;
            }
            if (reward.kind == RewardKind::Potion) {
                for (int i = 0; i < g.run.relics.size; ++i) if (relic_data(g.run.relics[i].id).blocks_potions) {
                    reward.done = true; g.screen_rewards.erase(action.index); return;
                }
                bool empty = false;
                for (int i = 0; i < g.run.potion_slots; ++i) empty |= g.run.potions[i] == PotionId::Empty;
                if (empty) {
                    for (int slot = 0; slot < g.run.potion_slots; ++slot) {
                        if (g.run.potions[slot] != PotionId::Empty) continue;
                        reward.potion_slot = slot;
                        break;
                    }
                    obtain_potion(g, reward.potion, reward.potion_state);
                    g.run.potion_identity[reward.potion_slot] = reward.identity;
                    reward.done = true;
                    g.screen_rewards.erase(action.index);
                }
                return;
            }
            if (reward.kind == RewardKind::Relic || reward.kind == RewardKind::SapphireKey || reward.kind == RewardKind::EmeraldKey) {
                if (reward.kind == RewardKind::SapphireKey) g.run.sapphire_key = true;
                else if (reward.kind == RewardKind::EmeraldKey) g.run.emerald_key = true;
                else {
                obtain_relic_object(g, g.screen_rewards[action.index]);
                }
                if (reward.relic_link >= 0) {
                    reward.ignored = true;
                    for (int i = 0; i < g.screen_rewards.size; ++i) {
                        auto& linked = g.reward_objects[g.screen_rewards[i]];
                        if (linked.identity == reward.relic_link) { linked.done = true; linked.ignored = true; }
                    }
                }
                reward.done = true;
                for (int i = g.screen_rewards.size - 1; i >= 0; --i)
                    if (g.reward_objects[g.screen_rewards[i]].done) g.screen_rewards.erase(i);
                settle_card_obtains(g);
                return;
            }
            SPIRE_ASSERT(reward.kind == RewardKind::Card);
            g.acquisition_reward = g.screen_rewards[action.index];
            g.reward_cards.clear();
            const auto& cards = reward.cards;
            for (int i = 0; i < cards.size; ++i) g.reward_cards.push_back(cards[i]);
            g.reward_skippable = true;
            g.previous_screen = Screen::CombatReward;
            g.screen = Screen::CardReward;
        } else {
            SPIRE_ASSERT(action.kind == ActionKind::Proceed || action.kind == ActionKind::Skip);
            int relic_count = 0;
            for (int i = 0; i < g.screen_rewards.size; ++i) relic_count += g.reward_objects[g.screen_rewards[i]].kind == RewardKind::Relic;
            // ProceedButton.update consumes the first attempt to leave three relic rewards with an FTUE.
            if (action.kind == ActionKind::Proceed && relic_count == 3 && !g.reward_relic_warning_seen) {
                g.reward_relic_warning_seen = true;
                g.previous_screen = Screen::CombatReward;
                return;
            }
            if (action.kind == ActionKind::Skip) {
                if (g.previous_screen == g.screen) g.previous_screen = Screen::None;
                g.screen = g.event.id != EventId::Count ? Screen::Event : Screen::Room;
            } else if (g.room_kind == RoomKind::BossTreasure) next_act(g);
            else if (g.room_kind == RoomKind::Boss && g.run.act < 3) enter_boss_treasure(g);
            else if (g.room_kind == RoomKind::Event && !event_data(g.event.id).reward_opens_map) {
                g.previous_screen = Screen::None;
                g.screen = Screen::Event;
                event_reopen(g);
                update_event(g);
            } else open_map(g);
        }
        return;
    }
    if (!g.room_in_combat && g.screen == Screen::CardReward) {
        if (action.kind == ActionKind::Choose) {
            if (action.index == g.reward_cards.size) increase_max_hp(g, 2);
            else obtain_card_object(g, g.reward_cards[action.index]);
            for (int i = 0; i < g.screen_rewards.size; ++i)
                if (g.screen_rewards[i] == g.acquisition_reward) { g.screen_rewards.erase(i); break; }
            g.reward_proceed_visible = !g.combat_reward_skippable || g.screen_rewards.empty();
        } else SPIRE_ASSERT(action.kind == ActionKind::Skip);
        g.screen = g.acquisition_reward >= 0 ? Screen::CombatReward : g.room_kind == RoomKind::Rest ? Screen::Room : Screen::Event;
        return;
    }
    if (g.screen == Screen::Grid && g.grid_confirm_screen) {
        if (action.kind == ActionKind::Skip) {
            g.grid_confirm_screen = false;
            g.grid_hovered_card = -1;
            return;
        }
        SPIRE_ASSERT(action.kind == ActionKind::Proceed);
        g.selected_cards.push_back(g.grid_hovered_card);
        g.grid_confirm_screen = false;
        g.grid_hovered_card = -1;
        complete_master_selection(g);
        return;
    }
    if (g.screen == Screen::Grid && action.kind == ActionKind::Skip) {
        SPIRE_ASSERT((g.room_kind == RoomKind::Rest || g.room_kind == RoomKind::Shop) && g.grid_can_cancel);
        if (g.room_kind == RoomKind::Shop) { g.screen = Screen::Shop; return; }
        g.campfire.something_selected = false;
        g.room_phase = RoomPhase::Incomplete;
        g.screen = Screen::Room;
        return;
    }
    if (g.screen == Screen::Grid && g.selection_confirmation) {
        if (action.kind == ActionKind::Choose) return;
        SPIRE_ASSERT(action.kind == ActionKind::Proceed);
        for (int i = 0; i < g.grid_cards.size; ++i) obtain_card_object(g, g.grid_cards[i]);
        g.selection_confirmation = false;
        complete_master_selection(g);
        return;
    }
    if (g.screen == Screen::Discard) {
        if (action.kind == ActionKind::Choose) {
            SPIRE_ASSERT(g.selected_cards.size < g.selection_amount);
            g.selected_cards.push_back(g.hand.erase(action.index));
            --g.discard_remaining;
            return;
        }
        SPIRE_ASSERT(action.kind == ActionKind::Proceed);
        SPIRE_ASSERT((g.selection_can_pick_zero && g.selected_cards.empty()) ||
            ((!g.selected_cards.empty() || g.selection_can_pick_zero) &&
                (g.selection_any_number || g.selection_up_to || g.selected_cards.size == g.selection_amount)));
        g.screen = Screen::Combat;
    } else if (g.screen == Screen::CardReward) {
        if (action.kind == ActionKind::Choose) g.discovery_card = g.reward_cards[action.index];
        else SPIRE_ASSERT(action.kind == ActionKind::Skip && g.reward_skippable);
        g.screen = Screen::Combat;
    } else if (g.screen == Screen::Grid) {
        if (action.kind == ActionKind::Choose) {
            const int card = get_pile(g, g.grid_source)[action.index];
            if (g.grid_for_upgrade || g.grid_for_transform || (g.grid_for_purge && g.selection_amount == 1)) {
                g.grid_hovered_card = card; g.grid_confirm_screen = true; return;
            }
            if (!remove_from_pile(g.selected_cards, card) && g.selected_cards.size < g.selection_amount)
                g.selected_cards.push_back(card);
            if (g.selected_cards.size != g.selection_amount ||
                (g.selection_any_number && g.selected_cards.size == get_pile(g, g.grid_source).size)) return;
        } else SPIRE_ASSERT(action.kind == ActionKind::Proceed && g.selection_any_number);
        if (!g.room_in_combat) {
            complete_master_selection(g);
            return;
        }
        g.screen = Screen::Combat;
    } else {
        SPIRE_ASSERT(g.screen == Screen::Combat);
        if (action.kind == ActionKind::EndTurn) {
            g.player.end_turn_queued = true;
            g.manager.card_queue.push_back({-1, -1});
        } else {
            SPIRE_ASSERT(action.kind == ActionKind::Play);
            const int card = g.hand[action.index];
            SPIRE_ASSERT(can_play(g, card));
            int target = no_target;
            if (g.cards[card].target == CardTarget::Enemy || g.cards[card].target == CardTarget::SelfAndEnemy) {
                target = g.monster_order[action.target];
                SPIRE_ASSERT(alive(g.monsters[target].creature));
                const Powers& powers = g.player.creature.powers;
                for (int i = 0; i < powers.size; ++i) {
                    if (power_data(powers.at(g.power_objects, i).id).rear_attack_power != PowerId::Count) {
                        g.player.creature.flip_horizontal = g.monsters[target].creature.draw_x < g.player.creature.draw_x;
                    }
                }
            }
            g.manager.card_queue.push_back({card, target, g.player.energy});
        }
    }
    run_until_decision(g);
}
}
