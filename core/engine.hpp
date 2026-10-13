#pragma once
#include "cards.hpp"
#include "monsters.hpp"
#include "lifecycle.hpp"

namespace spire {
SPIRE_FN inline void refresh_hand(Game& g) {
    if (monsters_basically_dead(g)) return;
    for (int i = 0; i < g.run.relics.size; ++i) relic_onRefreshHand(g, i);
}
SPIRE_FN inline void on_draw_or_discard(Game& g) {
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_onDrawOrDiscard(g, powers[i], player_target);
    for (int i = 0; i < g.run.relics.size; ++i) relic_onDrawOrDiscard(g, i);
    for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
}
SPIRE_FN inline void move_to_discard(Game& g, int card) {
    card_onMoveToDiscard(g, card);
    g.discard.push_back(card);
    on_draw_or_discard(g);
}
SPIRE_FN inline void discard_card(Game& g, int hand_index, bool end_turn) {
    const int card = g.hand.erase(hand_index);
    move_to_discard(g, card);
    if (!end_turn) card_triggerOnManualDiscard(g, card);
    ++g.manager.total_discarded_this_turn;
    if (!end_turn && !g.manager.turn_has_ended) {
        for (int i = 0; i < g.hand.size; ++i) card_didDiscard(g, g.hand[i]);
        for (int i = 0; i < g.discard.size; ++i) card_didDiscard(g, g.discard[i]);
        for (int i = 0; i < g.draw.size; ++i) card_didDiscard(g, g.draw[i]);
        for (int i = 0; i < g.run.relics.size; ++i) relic_onManualDiscard(g, i);
    }
}
SPIRE_FN inline void exhaust_card(Game& g, int card) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_onExhaust(g, i, card);
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_onExhaust(g, powers[i], player_target, card);
    card_triggerOnExhaust(g, card);
    for (int i = 0; i < g.hand.size; ++i) if (g.hand[i] == card) { g.hand.erase(i); break; }
    g.exhaust.push_back(card);
    on_draw_or_discard(g);
}
SPIRE_FN inline void finish_card_use(Game& g, const Effect& effect) {
    Effect use = effect;
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_onAfterUseCard(g, powers[i], player_target, use.card, use);
    for (int m = 0; m < g.monster_order.size; ++m) {
        const int owner = g.monster_order[m];
        Powers& other = g.monsters[owner].creature.powers;
        for (int i = 0; i < other.size; ++i) power_onAfterUseCard(g, other[i], owner, use.card, use);
    }
    Card& card = g.cards[use.card];
    card.free_to_play_once = false;
    g.player.card_in_use = -1;
    if (card_def(card.id).type != CardType::Power) {
        if (use.flag) exhaust_card(g, use.card);
        else move_to_discard(g, use.card);
    }
    card.exhaust_on_use_once = false;
    Effect check{};
    check.kind = EffectKind::HandCheck;
    add_bottom(g, check);
}
SPIRE_FN inline void clear_post_combat_actions(Game& g) {
    const int count = g.manager.actions.size;
    for (int i = 0; i < count; ++i) {
        const Effect e = g.manager.actions.pop_front();
        if (action_data(e.kind).survives_combat_end)
            g.manager.actions.push_back(e);
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
            for (int i = 0; i < c.powers.size; ++i) power_onDeath(g, c.powers[i], owner);
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
        for (int i = 0; i < source.size; ++i) amount = power_onAttackToChangeDamage(g, source[i], info.source, info, amount);
    }
    for (int i = 0; i < target.powers.size; ++i) amount = power_onAttackedToChangeDamage(g, target.powers[i], owner, info, amount);
    if (info.source == player_target)
        for (int i = 0; i < g.run.relics.size; ++i) relic_onAttack(g, i, info, amount, owner);
    for (int i = 0; i < target.powers.size; ++i) power_wasHPLost(g, target.powers[i], owner, info, amount);
    if (info.source != no_target) {
        Powers& source = get_creature(g, info.source).powers;
        for (int i = 0; i < source.size; ++i) power_onAttack(g, source[i], info.source, info, amount, owner);
    }
    for (int i = 0; i < target.powers.size; ++i) amount = power_onAttacked(g, target.powers[i], owner, info, amount);
    target.last_damage_taken = min(amount, target.hp);
    if (amount > 0) target.hp = max(0, target.hp - amount);
    if (target.hp <= 0) {
        monster_die(g, owner, true);
        target.block = 0;
    }
}
SPIRE_FN inline void player_damage(Game& g, const DamageInfo& info) {
    Creature& target = g.player.creature;
    int amount = decrement_block(g, player_target, info, max(0, info.amount));
    if (info.source == player_target)
        for (int i = 0; i < g.run.relics.size; ++i) amount = relic_onAttackToChangeDamage(g, i, info, amount);
    if (info.source != no_target) {
        Powers& source = get_creature(g, info.source).powers;
        for (int i = 0; i < source.size; ++i) amount = power_onAttackToChangeDamage(g, source[i], info.source, info, amount);
    }
    for (int i = 0; i < g.run.relics.size; ++i) amount = relic_onAttackedToChangeDamage(g, i, info, amount);
    for (int i = 0; i < target.powers.size; ++i) amount = power_onAttackedToChangeDamage(g, target.powers[i], player_target, info, amount);
    if (info.source == player_target)
        for (int i = 0; i < g.run.relics.size; ++i) relic_onAttack(g, i, info, amount, player_target);
    if (info.source != no_target) {
        Powers& source = get_creature(g, info.source).powers;
        for (int i = 0; i < source.size; ++i) power_onAttack(g, source[i], info.source, info, amount, player_target);
        for (int i = 0; i < target.powers.size; ++i) amount = power_onAttacked(g, target.powers[i], player_target, info, amount);
        for (int i = 0; i < g.run.relics.size; ++i) amount = relic_onAttacked(g, i, info, amount);
    }
    for (int i = 0; i < g.run.relics.size; ++i) amount = relic_onLoseHpLast(g, i, amount);
    target.last_damage_taken = min(amount, target.hp);
    if (amount > 0) {
        for (int i = 0; i < target.powers.size; ++i) amount = power_onLoseHp(g, target.powers[i], player_target, amount);
        for (int i = 0; i < g.run.relics.size; ++i) relic_onLoseHp(g, i, amount);
        for (int i = 0; i < target.powers.size; ++i) power_wasHPLost(g, target.powers[i], player_target, info, amount);
        for (int i = 0; i < g.run.relics.size; ++i) relic_wasHPLost(g, i, amount);
        if (info.source != no_target) {
            Powers& source = get_creature(g, info.source).powers;
            for (int i = 0; i < source.size; ++i) power_onInflictDamage(g, source[i], info.source, info, amount, player_target);
        }
        g.manager.damage_received_this_turn += amount;
        g.manager.damage_received_this_combat += amount;
        if (info.damage_type == DamageType::HpLoss) g.manager.hp_loss_this_combat += amount;
        target.hp -= amount;
        if (amount > 0) {
            ++g.player.damaged_this_combat;
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
            target.dead = true;
            target.block = 0;
            g.screen = Screen::Death;
        }
    }
}
SPIRE_FN inline void damage(Game& g, const Effect& e) {
    Creature& target = get_creature(g, e.target);
    if (!alive(target)) return;
    if (e.damage_type != DamageType::Thorns && e.source != no_target && !alive(get_creature(g, e.source))) return;
    if (e.extra != 0) g.run.gold -= min(g.run.gold, e.extra);
    if (e.target == player_target) player_damage(g, e);
    else {
        monster_damage(g, e.target, e);
        if (monsters_basically_dead(g)) {
            g.battle_over = true;
            g.manager.card_queue.clear();
            clear_post_combat_actions(g);
        }
    }
}
SPIRE_FN inline void heal_creature(Game& g, int owner, int amount) {
    Creature& c = get_creature(g, owner);
    if (c.dying) return;
    if (owner == player_target)
        for (int i = 0; i < g.run.relics.size; ++i) amount = relic_onPlayerHeal(g, i, amount);
    for (int i = 0; i < c.powers.size; ++i) amount = power_onHeal(g, c.powers[i], owner, amount);
    c.hp = min(c.max_hp, c.hp + amount);
    if (owner == player_target && c.hp > c.max_hp / 2 && g.player.bloodied) {
        g.player.bloodied = false;
        for (int i = 0; i < g.run.relics.size; ++i) relic_onNotBloodied(g, i);
    }
}
SPIRE_FN inline void monster_apply_powers(Game& g, int owner) {
    Monster& m = g.monsters[owner];
    if (m.pending_base_damage >= 0) m.intent_damage = calculate_damage(g, owner, player_target, m.pending_base_damage);
}
SPIRE_FN inline void draw_cards(Game& g, const Effect& effect) {
    if (!effect.flag) g.drawn_cards.clear();
    for (int i = 0; i < g.player.creature.powers.size; ++i)
        if (power_data(g.player.creature.powers[i].id).blocks_draw) return;
    if (effect.amount <= 0 ||
        g.draw.size + g.discard.size == 0 || g.hand.size == hand_limit) return;
    const int count = min(effect.amount, hand_limit - g.hand.size);
    if (count > g.draw.size) {
        Effect tail{};
        tail.kind = EffectKind::Draw;
        tail.amount = count - g.draw.size;
        tail.flag = true;
        add_top(g, tail);
        for (int i = 0; i < g.run.relics.size; ++i) relic_onShuffle(g, i);
        Effect shuffle_effect{};
        shuffle_effect.kind = EffectKind::Shuffle;
        add_top(g, shuffle_effect);
        if (!g.draw.empty()) {
            Effect head{};
            head.kind = EffectKind::Draw;
            head.amount = g.draw.size;
            head.flag = true;
            add_top(g, head);
        }
        return;
    }
    for (int i = 0; i < count; ++i) {
        const int card = g.draw[g.draw.size - 1];
        g.drawn_cards.push_back(card);
        card_triggerWhenDrawn(g, card);
        g.hand.push_back(card);
        g.draw.pop_back();
        Powers& powers = g.player.creature.powers;
        for (int j = 0; j < powers.size; ++j) power_onCardDraw(g, powers[j], player_target, card);
        for (int j = 0; j < g.run.relics.size; ++j) relic_onCardDraw(g, j, card);
        on_draw_or_discard(g);
        refresh_hand(g);
    }
}
SPIRE_FN inline void end_turn_discard(Game& g) {
    for (int i = 0; i < g.hand.size;) {
        const Card& card = g.cards[g.hand[i]];
        if (card.retain || card.self_retain) g.retained.push_back(g.hand.erase(i));
        else ++i;
    }
    Effect restore{};
    restore.kind = EffectKind::RestoreRetained;
    add_top(g, restore);
    const int count = g.hand.size;
    for (int i = 0; i < count; ++i) {
        Effect discard{};
        discard.kind = EffectKind::Discard;
        discard.amount = count;
        discard.flag = true;
        add_top(g, discard);
    }
    Pile shuffled = g.hand;
    // DiscardAtEndOfTurnAction.update uses an unseeded Collections.shuffle before ethereal hooks.
    shuffle_java(shuffled.values, shuffled.size, g.unseeded);
    for (int i = 0; i < shuffled.size; ++i) {
        const int card = shuffled[i];
        card_triggerOnEndOfPlayerTurn(g, card);
    }
}
SPIRE_FN inline void execute_effect(Game& g, const Effect& e) { action_execute(g, e); }
SPIRE_FN inline void begin_end_turn(Game& g) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_onPlayerEndTurn(g, i);
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_atEndOfTurnPreEndTurnCards(g, powers[i], player_target, true);
    for (int i = 0; i < g.hand.size; ++i) card_triggerOnEndOfTurnForPlayingCard(g, g.hand[i]);
    at_end_of_turn(g, player_target);
    Effect discard{};
    discard.kind = EffectKind::EndTurnDiscard;
    add_bottom(g, discard);
    for (int i = 0; i < g.draw.size; ++i) card_resetAttributes(g, g.draw[i]);
    for (int i = 0; i < g.discard.size; ++i) card_resetAttributes(g, g.discard[i]);
    for (int i = 0; i < g.hand.size; ++i) card_resetAttributes(g, g.hand[i]);
    Effect end{};
    end.kind = EffectKind::EndTurn;
    add_bottom(g, end);
    Effect start{};
    start.kind = EffectKind::MonsterStart;
    add_bottom(g, start);
}
SPIRE_FN inline void recharge_energy(Game& g) {
    g.player.energy = g.player.energy_master;
    g.manager.energy_gained_this_combat += g.player.energy_master;
    for (int i = 0; i < g.run.relics.size; ++i) relic_onEnergyRecharge(g, i);
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_onEnergyRecharge(g, powers[i], player_target);
}
SPIRE_FN inline void after_draw_turn_hooks(Game& g) {
    for (int i = 0; i < g.run.relics.size; ++i) relic_atTurnStartPostDraw(g, i);
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_atStartOfTurnPostDraw(g, powers[i], player_target);
}
SPIRE_FN inline void begin_player_turn(Game& g) {
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int id = g.monster_order[i];
        if (!g.monsters[id].creature.dying && !g.monsters[id].creature.escaped) at_end_of_turn(g, id);
    }
    at_end_of_round(g, player_target);
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int id = g.monster_order[i];
        if (!g.monsters[id].creature.dying && !g.monsters[id].creature.escaped) at_end_of_round(g, id);
    }
    g.player.cards_played_this_turn = 0;
    for (int i = 0; i < g.run.relics.size; ++i) relic_atTurnStart(g, i);
    for (int i = 0; i < g.hand.size; ++i) card_atTurnStartPreDraw(g, g.hand[i]);
    for (int i = 0; i < g.draw.size; ++i) card_atTurnStart(g, g.draw[i]);
    for (int i = 0; i < g.hand.size; ++i) card_atTurnStart(g, g.hand[i]);
    for (int i = 0; i < g.discard.size; ++i) card_atTurnStart(g, g.discard[i]);
    Powers& powers = g.player.creature.powers;
    for (int i = 0; i < powers.size; ++i) power_atStartOfTurn(g, powers[i], player_target);
    ++g.turn;
    g.manager.turn_has_ended = false;
    g.manager.total_discarded_this_turn = 0;
    g.manager.cards_played_this_turn.clear();
    g.manager.damage_received_this_turn = 0;
    g.player.creature.block = 0;
    recharge_energy(g);
    for (int i = 0; i < g.monster_order.size; ++i) monster_createIntent(g, g.monster_order[i]);
    Effect draw{};
    draw.kind = EffectKind::Draw;
    draw.amount = g.player.hand_size;
    add_bottom(g, draw);
    after_draw_turn_hooks(g);
    refresh_powers(g);
}
SPIRE_FN inline void run_until_decision(Game& g) {
    g.manager.phase = Phase::ExecutingActions;
    while (g.screen != Screen::Discard && g.screen != Screen::Death) {
        if (!g.manager.actions.empty()) {
            const Effect next = g.manager.actions.pop_front();
            execute_effect(g, next);
        } else if (!g.manager.pre_turn_actions.empty()) {
            const Effect next = g.manager.pre_turn_actions.pop_front();
            execute_effect(g, next);
        } else if (!g.manager.card_queue.empty()) {
            const CardQueueItem queued = g.manager.card_queue.pop_front();
            if (queued.card < 0) {
                begin_end_turn(g);
            } else {
                play_card_hooks(g, queued.card, queued.target);
                ++g.player.cards_played_this_turn;
                g.manager.cards_played_this_turn.record(queued.card, card_def(g.cards[queued.card].id).type);
                g.manager.cards_played_this_combat.record(queued.card, card_def(g.cards[queued.card].id).type);
                use_card(g, queued.card, queued.target);
            }
        } else if (g.battle_over) {
            on_victory(g);
            g.screen = Screen::Victory;
            g.manager.phase = Phase::WaitingOnUser;
            break;
        } else if (!g.manager.monster_attacks_queued) {
            g.manager.monster_attacks_queued = true;
            for (int i = 0; i < g.monster_order.size; ++i) {
                const int id = g.monster_order[i];
                const Creature& c = g.monsters[id].creature;
                if (alive(c) || c.half_dead) g.manager.monster_queue.push_back(id);
            }
        } else if (!g.manager.monster_queue.empty()) {
            const int id = g.manager.monster_queue.pop_front();
            if (alive(g.monsters[id].creature) || g.monsters[id].creature.half_dead) {
                monster_take_turn(g, id);
                Powers& powers = g.monsters[id].creature.powers;
                for (int i = 0; i < powers.size; ++i) power_duringTurn(g, powers[i], id);
            }
        } else if (g.manager.turn_has_ended && !monsters_basically_dead(g)) {
            begin_player_turn(g);
        } else {
            g.manager.phase = Phase::WaitingOnUser;
            refresh_hand(g);
            if (!g.manager.actions.empty()) {
                g.manager.phase = Phase::ExecutingActions;
                continue;
            }
            break;
        }
    }
}
SPIRE_FN inline void reset(Game& g, uint64_t seed) {
    g = Game{};
    g.run.seed = seed;
    g.rng.generate_seeds(seed);
    g.rng.reseed_map(seed, 1);
    g.player.creature.max_hp = 66;
    g.player.creature.hp = 59;
    starter_deck(g.run);
}
SPIRE_FN inline void start_combat(Game& g, const CombatSetup& setup) {
    g.run.floor = setup.floor;
    g.run.gold = setup.gold;
    g.run.master_deck = setup.deck;
    g.run.relics = setup.relics;
    for (int i = 0; i < 2; ++i) {
        g.run.potions[i] = setup.potions[i];
        potion_initialize_state(g, i);
    }
    g.rng.reseed_floor(g.run.seed, setup.floor);
    g.unseeded = JavaRandom(setup.unseeded_seed);
    g.player = Player{};
    g.player.creature.hp = setup.hp;
    g.player.creature.max_hp = setup.max_hp;
    g.player.bloodied = setup.hp <= setup.max_hp / 2;
    g.cards.clear();
    g.hand.clear(); g.draw.clear(); g.discard.clear(); g.exhaust.clear(); g.retained.clear();
    g.drawn_cards.clear();
    g.monsters.clear();
    g.monster_order.clear();
    g.manager = ActionManager{};
    g.manager.player_hp_last_turn = setup.hp;
    g.turn = 1;
    g.battle_over = false;
    g.mugged = false;
    g.discard_remaining = 0;
    g.screen = Screen::Combat;
    g.encounter = setup.encounter;
    encounter_initialize_state(g);
    build_encounter(g, setup.encounter);
    for (int i = 0; i < g.monster_order.size; ++i) roll_monster_move(g, g.monster_order[i]);
    Pile copy{};
    for (int i = 0; i < g.run.master_deck.size; ++i) {
        g.cards.push_back(g.run.master_deck[i]);
        copy.push_back(i);
    }
    shuffle(copy.values, copy.size, g.rng.shuffle);
    Pile innate{};
    for (int i = 0; i < copy.size; ++i) {
        const int card = copy[i];
        if (g.cards[card].innate) innate.push_back(card);
        else g.draw.push_back(card);
    }
    for (int i = 0; i < innate.size; ++i) g.draw.push_back(innate[i]);
    if (innate.size > g.player.hand_size) {
        Effect draw{};
        draw.kind = EffectKind::Draw;
        draw.amount = innate.size - g.player.hand_size;
        g.manager.pre_turn_actions.push_front(draw);
    }
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int owner = g.monster_order[i];
        monster_pre_battle(g, owner);
        monster_useUniversalPreBattleAction(g, owner);
    }
    for (int i = 0; i < g.run.relics.size; ++i) relic_atPreBattle(g, i);
    for (int i = 0; i < g.run.relics.size; ++i) relic_atBattleStartPreDraw(g, i);
    run_until_decision(g);
    recharge_energy(g);
    Effect draw{};
    draw.kind = EffectKind::Draw;
    draw.amount = g.player.hand_size;
    add_bottom(g, draw);
    relics_at_battle_start(g);
    run_until_decision(g);
    for (int i = 0; i < g.monster_order.size; ++i) monster_createIntent(g, g.monster_order[i]);
    refresh_powers(g);
}
SPIRE_FN inline void legal_actions(const Game& g, ActionSet& actions) {
    actions.clear();
    if (g.screen == Screen::Discard) {
        for (int i = 0; i < g.hand.size; ++i) actions.push_back({ActionKind::Choose, i, -1});
        return;
    }
    if (g.screen != Screen::Combat) return;
    for (int i = 0; i < g.hand.size; ++i) {
        const Card& card = g.cards[g.hand[i]];
        if (!can_play(g, card)) continue;
        if (card_def(card.id).target == CardTarget::Enemy) {
            for (int j = 0; j < g.monster_order.size; ++j) {
                if (alive(g.monsters[g.monster_order[j]].creature) && card_canUse(g, g.hand[i], g.monster_order[j])) actions.push_back({ActionKind::Play, i, j});
            }
        } else actions.push_back({ActionKind::Play, i, -1});
    }
    for (int slot = 0; slot < 2; ++slot) {
        const PotionData data = potion_data(g.run.potions[slot]);
        if (!data.usable) continue;
        if (potion_canUse(g, slot)) {
            if (data.target == CardTarget::Enemy) {
                for (int i = 0; i < g.monster_order.size; ++i)
                    if (alive(g.monsters[g.monster_order[i]].creature)) actions.push_back({ActionKind::PotionUse, slot, i});
            } else actions.push_back({ActionKind::PotionUse, slot, -1});
        }
        actions.push_back({ActionKind::PotionDiscard, slot, -1});
    }
    actions.push_back({ActionKind::EndTurn, -1, -1});
}
SPIRE_FN inline void step(Game& g, Action action) {
    if (g.screen == Screen::Discard) {
        SPIRE_ASSERT(action.kind == ActionKind::Choose);
        discard_card(g, action.index, false);
        --g.discard_remaining;
        if (g.discard_remaining > 0) return;
        g.screen = Screen::Combat;
    } else {
        SPIRE_ASSERT(g.screen == Screen::Combat);
        if (action.kind == ActionKind::EndTurn) {
            g.manager.card_queue.push_back({-1, -1});
        } else if (action.kind == ActionKind::PotionUse) {
            SPIRE_ASSERT(potion_data(g.run.potions[action.index]).usable && potion_canUse(g, action.index));
            const int target = action.target >= 0 ? g.monster_order[action.target] : player_target;
            potion_use(g, action.index, target);
            for (int i = 0; i < g.run.relics.size; ++i) relic_onUsePotion(g, i);
            g.run.potions[action.index] = PotionId::Empty;
        } else if (action.kind == ActionKind::PotionDiscard) {
            g.run.potions[action.index] = PotionId::Empty;
        } else {
            SPIRE_ASSERT(action.kind == ActionKind::Play);
            const int card = g.hand[action.index];
            SPIRE_ASSERT(can_play(g, g.cards[card]));
            int target = no_target;
            if (card_def(g.cards[card].id).target == CardTarget::Enemy) {
                target = g.monster_order[action.target];
                SPIRE_ASSERT(alive(g.monsters[target].creature));
            }
            g.manager.card_queue.push_back({card, target});
        }
    }
    run_until_decision(g);
}
}
