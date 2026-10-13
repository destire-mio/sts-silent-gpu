#pragma once
#include "cards.hpp"
#include "monsters.hpp"

namespace spire {
SPIRE_FN inline void discard_card(Game& g, int hand_index, bool end_turn) {
    (void)end_turn;
    g.discard.push_back(g.hand.erase(hand_index));
    ++g.manager.total_discarded_this_turn;
    refresh_powers(g);
}
SPIRE_FN inline void clear_post_combat_actions(Game& g) {
    const int count = g.manager.actions.size;
    for (int i = 0; i < count; ++i) {
        const Effect e = g.manager.actions.pop_front();
        if (e.kind == EffectKind::Damage || e.kind == EffectKind::Block || e.kind == EffectKind::UseCard)
            g.manager.actions.push_back(e);
    }
}
SPIRE_FN inline void damage(Game& g, const Effect& e) {
    Creature& target = get_creature(g, e.target);
    if (!alive(target)) return;
    if (e.damage_type != DamageType::Thorns && e.source != no_target && !alive(get_creature(g, e.source))) return;
    if (e.extra != 0) g.run.gold -= min(g.run.gold, e.extra);
    int amount = max(0, e.amount);
    if (e.damage_type != DamageType::HpLoss) {
        const int blocked = min(target.block, amount);
        target.block -= blocked;
        amount -= blocked;
    }
    for (int i = 0; i < target.powers.size; ++i) {
        Power& p = target.powers[i];
        if (p.id == PowerId::CurlUp && !p.triggered && amount > 0 && amount < target.hp &&
            e.source != no_target && e.damage_type == DamageType::Normal) {
            p.triggered = true;
            queue_block(g, e.target, p.amount);
            Effect remove{};
            remove.kind = EffectKind::RemovePower;
            remove.target = e.target;
            remove.power = p.id;
            add_bottom(g, remove);
        }
        if (p.id == PowerId::Angry && amount > 0 && e.source != no_target && e.damage_type == DamageType::Normal) {
            Effect strength{};
            strength.kind = EffectKind::ApplyPower;
            strength.source = e.target;
            strength.target = e.target;
            strength.power = PowerId::Strength;
            strength.amount = p.amount;
            add_top(g, strength);
        }
    }
    target.last_damage_taken = min(amount, target.hp);
    target.hp = max(0, target.hp - amount);
    if (e.target == player_target) {
        if (amount > 0) {
            ++g.player.damaged_this_combat;
            g.manager.damage_received_this_turn += amount;
            g.manager.damage_received_this_combat += amount;
            if (e.damage_type == DamageType::HpLoss) g.manager.hp_loss_this_combat += amount;
        }
        if (target.hp == 0) {
            target.dead = true;
            target.block = 0;
            g.screen = Screen::Death;
        }
    } else {
        if (target.hp == 0) {
            target.dying = true;
            target.dead = true;
            target.block = 0;
            for (int i = 0; i < target.powers.size; ++i) {
                const Power& p = target.powers[i];
                if (p.id == PowerId::SporeCloud && !monsters_basically_dead(g)) {
                    Effect spore{};
                    spore.kind = EffectKind::ApplyPower;
                    spore.source = no_target;
                    spore.target = player_target;
                    spore.power = PowerId::Vulnerable;
                    spore.amount = p.amount;
                    spore.flag = g.manager.turn_has_ended;
                    add_top(g, spore);
                }
            }
        }
        monster_on_damage(g, e.target);
        if (monsters_basically_dead(g)) {
            g.battle_over = true;
            g.manager.card_queue.clear();
            clear_post_combat_actions(g);
        }
    }
}
SPIRE_FN inline void draw_cards(Game& g, const Effect& effect) {
    if (!effect.flag) g.drawn_cards.clear();
    if (has_power(g.player.creature, PowerId::NoDraw) || effect.amount <= 0 ||
        g.draw.size + g.discard.size == 0 || g.hand.size == hand_limit) return;
    const int count = min(effect.amount, hand_limit - g.hand.size);
    if (count > g.draw.size) {
        Effect tail{};
        tail.kind = EffectKind::Draw;
        tail.amount = count - g.draw.size;
        tail.flag = true;
        add_top(g, tail);
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
        const int card = g.draw.pop_back();
        g.drawn_cards.push_back(card);
        g.hand.push_back(card);
        refresh_powers(g);
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
        if (g.cards[card].ethereal) {
            Effect exhaust{};
            exhaust.kind = EffectKind::ExhaustCard;
            exhaust.card = card;
            add_top(g, exhaust);
        }
    }
}
SPIRE_FN inline void execute_effect(Game& g, const Effect& e) {
    switch (e.kind) {
        case EffectKind::Damage: damage(g, e); break;
        case EffectKind::Block: gain_block(g, e.target, e.amount); break;
        case EffectKind::ApplyPower: apply_power(g, e.source, e.target, e.power, e.amount, e.flag); break;
        case EffectKind::ReducePower: reduce_power(g, e.target, e.power, e.amount); break;
        case EffectKind::RemovePower: remove_power(g, e.target, e.power); break;
        case EffectKind::Draw: draw_cards(g, e); break;
        case EffectKind::Shuffle:
            shuffle(g.discard.values, g.discard.size, g.rng.shuffle);
            for (int i = 0; i < g.discard.size; ++i) g.draw.push_back(g.discard[i]);
            g.discard.clear();
            break;
        case EffectKind::Discard:
            if (monsters_basically_dead(g)) break;
            if (g.hand.size <= e.amount) {
                while (!g.hand.empty()) discard_card(g, g.hand.size - 1, e.flag);
            } else {
                SPIRE_ASSERT(!e.flag);
                g.discard_remaining = e.amount;
                g.screen = Screen::Discard;
            }
            break;
        case EffectKind::UseCard: {
            Card& card = g.cards[e.card];
            card.free_to_play_once = false;
            g.player.card_in_use = -1;
            if (e.flag) g.exhaust.push_back(e.card);
            else g.discard.push_back(e.card);
            card.exhaust_on_use_once = false;
            refresh_powers(g);
            break;
        }
        case EffectKind::EndTurnDiscard: end_turn_discard(g); break;
        case EffectKind::RestoreRetained:
            for (int i = 0; i < g.retained.size; ++i) {
                const int card = g.retained[i];
                g.hand.push_back(card);
                g.cards[card].retain = false;
            }
            g.retained.clear();
            break;
        case EffectKind::EndTurn:
            g.manager.turn_has_ended = true;
            g.manager.player_hp_last_turn = g.player.creature.hp;
            g.manager.monster_attacks_queued = false;
            break;
        case EffectKind::MonsterStart:
            for (int i = 0; i < g.monster_order.size; ++i) {
                Creature& c = g.monsters[g.monster_order[i]].creature;
                if (!c.dying && !c.escaped) c.block = 0;
            }
            break;
        case EffectKind::RollMove: roll_monster_move(g, e.target); break;
        case EffectKind::SetMove:
            set_monster_move(g, e.target, e.amount, static_cast<Intent>(e.extra), e.card);
            break;
        case EffectKind::Escape:
            g.monsters[e.target].creature.escaped = true;
            if (monsters_basically_dead(g)) g.battle_over = true;
            break;
        case EffectKind::AddCardDiscard:
            for (int i = 0; i < e.amount; ++i) {
                const int card = g.cards.size;
                g.cards.push_back(make_card(static_cast<CardId>(e.card)));
                g.discard.push_back(card);
            }
            break;
        case EffectKind::Split: split_monster(g, e.target); break;
        case EffectKind::StealGold: g.monsters[e.target].stolen_gold += min(e.amount, g.run.gold); break;
        case EffectKind::BlockRandomMonster: block_random_monster(g, e); break;
        case EffectKind::ExhaustCard:
            for (int i = 0; i < g.hand.size; ++i) {
                if (g.hand[i] != e.card) continue;
                g.exhaust.push_back(g.hand.erase(i));
                refresh_powers(g);
                break;
            }
            break;
    }
}
SPIRE_FN inline void begin_end_turn(Game& g) {
    at_end_of_turn(g, player_target);
    Effect discard{};
    discard.kind = EffectKind::EndTurnDiscard;
    add_bottom(g, discard);
    for (int i = 0; i < g.draw.size; ++i) reset_attributes(g.cards[g.draw[i]]);
    for (int i = 0; i < g.discard.size; ++i) reset_attributes(g.cards[g.discard[i]]);
    for (int i = 0; i < g.hand.size; ++i) reset_attributes(g.cards[g.hand[i]]);
    Effect end{};
    end.kind = EffectKind::EndTurn;
    add_bottom(g, end);
    Effect start{};
    start.kind = EffectKind::MonsterStart;
    add_bottom(g, start);
}
SPIRE_FN inline void begin_player_turn(Game& g) {
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int id = g.monster_order[i];
        if (alive(g.monsters[id].creature)) at_end_of_turn(g, id);
    }
    at_end_of_round(g, player_target);
    for (int i = 0; i < g.monster_order.size; ++i) {
        const int id = g.monster_order[i];
        if (alive(g.monsters[id].creature)) at_end_of_round(g, id);
    }
    g.player.cards_played_this_turn = 0;
    ++g.turn;
    g.manager.turn_has_ended = false;
    g.manager.total_discarded_this_turn = 0;
    g.manager.cards_played_this_turn.clear();
    g.manager.damage_received_this_turn = 0;
    g.player.creature.block = 0;
    g.player.energy = g.player.energy_master;
    g.manager.energy_gained_this_combat += g.player.energy_master;
    for (int i = 0; i < g.monster_order.size; ++i) create_monster_intent(g, g.monster_order[i]);
    Effect draw{};
    draw.kind = EffectKind::Draw;
    draw.amount = g.player.hand_size;
    add_bottom(g, draw);
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
                ++g.player.cards_played_this_turn;
                g.manager.cards_played_this_turn.push_back(queued.card);
                g.manager.cards_played_this_combat.push_back(queued.card);
                use_card(g, queued.card, queued.target);
            }
        } else if (g.battle_over) {
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
            if (alive(g.monsters[id].creature) || g.monsters[id].creature.half_dead) monster_take_turn(g, id);
        } else if (g.manager.turn_has_ended && !monsters_basically_dead(g)) {
            begin_player_turn(g);
        } else {
            g.manager.phase = Phase::WaitingOnUser;
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
        SPIRE_ASSERT(setup.potions[i] == PotionId::Empty);
        g.run.potions[i] = setup.potions[i];
    }
    g.rng.reseed_floor(g.run.seed, setup.floor);
    g.unseeded = JavaRandom(setup.unseeded_seed);
    g.player = Player{};
    g.player.creature.hp = setup.hp;
    g.player.creature.max_hp = setup.max_hp;
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
    for (int i = 0; i < g.monster_order.size; ++i) monster_pre_battle(g, g.monster_order[i]);
    run_until_decision(g);
    g.player.energy = g.player.energy_master;
    g.manager.energy_gained_this_combat += g.player.energy_master;
    Effect draw{};
    draw.kind = EffectKind::Draw;
    draw.amount = g.player.hand_size;
    add_bottom(g, draw);
    relics_at_battle_start(g);
    run_until_decision(g);
    for (int i = 0; i < g.monster_order.size; ++i) create_monster_intent(g, g.monster_order[i]);
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
                if (alive(g.monsters[g.monster_order[j]].creature)) actions.push_back({ActionKind::Play, i, j});
            }
        } else actions.push_back({ActionKind::Play, i, -1});
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
