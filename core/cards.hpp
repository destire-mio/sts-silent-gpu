#pragma once
#include "combat.hpp"

namespace spire {
SPIRE_FN inline bool card_free_to_play_base(const Game& g, int card) {
    if (g.cards[card].free_to_play_once) return true;
    if (card_data(g.cards[card].id).type == CardType::Attack) {
        const Powers& powers = g.player.creature.powers;
        for (int i = 0; i < powers.size; ++i) if (power_data(powers[i].id).free_attacks) return true;
    }
    return false;
}
SPIRE_FN inline bool card_has_enough_energy_base(const Game& g, int card_id) {
    if (g.manager.turn_has_ended) return false;
    const Card& card = g.cards[card_id];
    const Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i)
        if (!power_canPlayCard(g, powers[i], player_target, card_id)) return false;
    if (card_def(card.id).type == CardType::Attack)
        for (int i = 0; i < powers.size; ++i) if (power_data(powers[i].id).prevents_attacks) return false;
    for (int i = 0; i < g.run.relics.size; ++i)
        if (!relic_canPlay(g, i, card_id)) return false;
    for (int i = 0; i < g.hand.size; ++i)
        if (!card_canPlay(g, g.hand[i], card_id)) return false;
    return card.cost_for_turn <= g.player.energy || card_freeToPlay(g, card_id);
}
SPIRE_FN inline bool card_can_use_base(const Game& g, int card_id, int target) {
    const Card& c = g.cards[card_id];
    const CardType type = card_data(c.id).type;
    if (c.cost_for_turn < -1 && (type == CardType::Status || type == CardType::Curse)) {
        bool allowed = false;
        for (int i = 0; i < g.run.relics.size; ++i) {
            const RelicData data = relic_data(g.run.relics[i].id);
            if (type == CardType::Status ? data.can_play_status : data.can_play_curse) allowed = true;
        }
        if (!allowed) return false;
    }
    return card_cardPlayable(g, card_id, target) && card_hasEnoughEnergy(g, card_id);
}
SPIRE_FN inline bool can_play(const Game& g, int card) {
    return card_canUse(g, card, no_target);
}
SPIRE_FN inline Card stat_equivalent_copy(const Game& g, int card_id) {
    const Card& source = g.cards[card_id];
    Card copy = make_card(source.id, source.upgrades, source.misc);
    copy.base_damage = source.base_damage;
    copy.base_block = source.base_block;
    copy.base_magic = source.base_magic;
    copy.cost = source.cost;
    copy.cost_for_turn = source.cost_for_turn;
    copy.free_to_play_once = source.free_to_play_once;
    return copy;
}
SPIRE_FN inline int card_damage_value(Game& g, int card_id, int target) {
    Card& c = g.cards[card_id];
    float value = static_cast<float>(c.base_damage);
    for (int i = 0; i < g.run.relics.size; ++i) {
        value = relic_atDamageModify(g, i, value, card_id);
    }
    const Powers& source = g.player.creature.powers;
    for (int i = 0; i < source.size; ++i)
        value = power_atDamageGive(g, source[i], player_target, value, DamageType::Normal, card_id);
    if (target != no_target) {
        const Powers& receive = g.monsters[target].creature.powers;
        for (int i = 0; i < receive.size; ++i)
            value = power_atDamageReceive(g, receive[i], target, value, DamageType::Normal, card_id);
    }
    for (int i = 0; i < source.size; ++i)
        value = power_atDamageFinalGive(g, source[i], player_target, value, DamageType::Normal, card_id);
    if (target != no_target) {
        const Powers& receive = g.monsters[target].creature.powers;
        for (int i = 0; i < receive.size; ++i)
            value = power_atDamageFinalReceive(g, receive[i], target, value, DamageType::Normal, card_id);
    }
    const int damage = max(0, floor_int(value));
    return damage;
}
SPIRE_FN inline void apply_card_powers(Game& g, int card_id) {
    Card& c = g.cards[card_id];
    card_applyPowersToBlock(g, card_id);
    c.damage = card_damage_value(g, card_id, no_target);
}
SPIRE_FN inline void calculate_card_damage(Game& g, int card_id, int target) {
    Card& c = g.cards[card_id];
    card_applyPowersToBlock(g, card_id);
    if (target != no_target) c.damage = card_damage_value(g, card_id, target);
    else {
        // AbstractCard.calculateCardDamage(null) fills multiDamage even for skills.
        c.multi_damage_count = g.monster_order.size;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int owner = g.monster_order[i];
            const Creature& monster = g.monsters[owner].creature;
            g.multi_damage[card_id][i] = card_damage_value(g, card_id, monster.dying || monster.escaped ? no_target : owner);
        }
        c.damage = g.multi_damage[card_id][0];
    }
}
SPIRE_FN inline void queue_card_damage(Game& g, int card, int target) {
    Effect e{};
    e.kind = EffectKind::Damage;
    e.source = player_target;
    e.target = target;
    e.amount = g.cards[card].damage;
    e.info_base = e.amount;
    add_bottom(g, e);
}
SPIRE_FN inline void play_card_hooks(Game& g, int card, int target) {
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_onPlayCard(g, powers[i], player_target, card, target);
    for (int m = 0; m < g.monster_order.size; ++m) {
        const int owner = g.monster_order[m];
        Powers& others = g.monsters[owner].creature.powers;
        for (int i = 0; i < others.size; ++i) power_onPlayCard(g, others[i], owner, card, target);
    }
    for (int i = 0; i < g.run.relics.size; ++i) relic_onPlayCard(g, i, card, target);
    for (int i = 0; i < g.hand.size; ++i) card_onPlayCard(g, g.hand[i], card, target);
    for (int i = 0; i < g.discard.size; ++i) card_onPlayCard(g, g.discard[i], card, target);
    for (int i = 0; i < g.draw.size; ++i) card_onPlayCard(g, g.draw[i], card, target);
}
SPIRE_FN inline void use_card(Game& g, int card_id, int target) {
    Card& card = g.cards[card_id];
    card_calculateCardDamage(g, card_id, target);
    card_use(g, card_id, target);
    Effect finish{};
    finish.kind = EffectKind::UseCard;
    finish.card = card_id;
    finish.target = target;
    finish.flag = card.exhaust || card.exhaust_on_use_once;
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_onUseCard(g, powers[i], player_target, card_id, finish);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onUseCard(g, i, card_id, finish);
    for (int i = 0; i < g.hand.size; ++i) card_triggerOnCardPlayed(g, g.hand[i], card_id);
    for (int i = 0; i < g.discard.size; ++i) card_triggerOnCardPlayed(g, g.discard[i], card_id);
    for (int i = 0; i < g.draw.size; ++i) card_triggerOnCardPlayed(g, g.draw[i], card_id);
    for (int m = 0; m < g.monster_order.size; ++m) {
        const int owner = g.monster_order[m];
        Powers& others = g.monsters[owner].creature.powers;
        for (int i = 0; i < others.size; ++i) power_onUseCard(g, others[i], owner, card_id, finish);
    }
    add_bottom(g, finish);
    for (int i = 0; i < g.hand.size; ++i)
        if (g.hand[i] != card_id) card_triggerOnOtherCardPlayed(g, g.hand[i], card_id);
    for (int i = 0; i < powers.size; ++i) power_onAfterCardPlayed(g, powers[i], player_target, card_id);
    int hand_index = 0;
    while (g.hand[hand_index] != card_id) ++hand_index;
    g.hand.erase(hand_index);
    g.player.card_in_use = card_id;
    if (!card.free_to_play_once && card.cost_for_turn > 0) g.player.energy -= card.cost_for_turn;
}
SPIRE_FN inline void starter_deck(Run& run) {
    run.master_deck.clear();
    for (int i = 0; i < 5; ++i) run.master_deck.push_back(make_card(CardId::Strike_G));
    for (int i = 0; i < 5; ++i) run.master_deck.push_back(make_card(CardId::Defend_G));
    run.master_deck.push_back(make_card(CardId::Survivor));
    run.master_deck.push_back(make_card(CardId::Neutralize));
    run.master_deck.push_back(make_card(CardId::AscendersBane));
    run.relics.push_back({RelicId::SnakeRing, -1});
}
SPIRE_FN inline void relics_at_battle_start(Game& g) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_atBattleStart(g, i);
}
}
