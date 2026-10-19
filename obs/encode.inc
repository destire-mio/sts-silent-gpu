#pragma once
#include "obs/events.hpp"

namespace spire::obs {
SPIRE_FN inline int selection_prompt(const Game& g) {
    if (g.screen == Screen::Discard && g.selection_for_upgrade) return 1;
    if (g.screen == Screen::Grid) {
        if (g.grid_for_upgrade) return 1;
        if (g.grid_for_purge) return 2;
        if (g.grid_for_transform) return 3;
    }
    if (!g.manager.has_current_action) return 0;
    switch (g.action_objects[g.manager.current_action].kind) {
    case EffectKind::Discard: case EffectKind::GamblingChipAction: return 4;
    case EffectKind::Exhaust: case EffectKind::Recycle: return 5;
    case EffectKind::RetainCards: return 6;
    case EffectKind::PutOnDeck: case EffectKind::DiscardPileToTopOfDeck: return 7;
    case EffectKind::Forethought: return 8;
    case EffectKind::DualWield: return 9;
    case EffectKind::NightmareAction: return 10;
    case EffectKind::Setup: return 11;
    case EffectKind::Scry: return 12;
    case EffectKind::Omniscience: return 13;
    case EffectKind::BetterDiscardPileToHand: case EffectKind::BetterDrawPileToHand:
    case EffectKind::AttackFromDeckToHand: case EffectKind::SkillFromDeckToHand:
    case EffectKind::Exhume: return 14;
    case EffectKind::Meditate: return 15;
    default: return 0;
    }
}
SPIRE_FN inline void shop_entities(Writer& w, const Game& g, int* options) {
    int stock[14]{};
    if (g.shop.purge_available) {
        Token t = option(Control::Purge, 1); t.values[0] = g.run.actual_purge_cost;
        stock[0] = w.add(t);
    }
    for (int i = 0; i < g.shop.colored.size + g.shop.colorless.size; ++i) {
        const bool colorless = i >= g.shop.colored.size;
        const int pos = colorless ? i - g.shop.colored.size : i;
        const Card& c = g.cards[colorless ? g.shop.colorless[pos] : g.shop.colored[pos]];
        Token t = card_token(g, c, Zone::Shop, (colorless ? 6 : 1) + pos);
        t.values[11] = c.price;
        stock[(colorless ? 6 : 1) + pos] = w.add(t);
    }
    for (int i = 0; i < g.shop.relics.size; ++i) {
        const auto& item = g.shop.relics[i];
        Token t = relic_token(g.reward_objects[item.reward].relic, Zone::Shop, item.slot + 1, false);
        t.values[11] = item.price; stock[8 + i] = w.add(t);
    }
    for (int i = 0; i < g.shop.potions.size; ++i) {
        const auto& item = g.shop.potions[i];
        Token t = token(Kind::Potion, static_cast<int>(g.reward_objects[item.reward].potion) + 1, Zone::Shop, item.slot + 1);
        t.values[11] = item.price; stock[11 + i] = w.add(t);
    }
    List<uint8_t, 14> choices{}; shop_choices(g, choices);
    for (int i = 0; i < choices.size; ++i) options[i] = stock[choices[i]];
}
SPIRE_FN inline void map_entities(Writer& w, const Game& g, int* options) {
    int nodes[106]{};
    for (int y = 0; y < 15; ++y) for (int x = 0; x < 7; ++x) {
        const auto& n = g.run.map[y][x];
        if (!n.edges) continue;
        Token t = token(Kind::MapNode, static_cast<int>(n.room) + 1);
        t.values[0] = x; t.values[1] = y; t.values[2] = n.edges;
        t.values[3] = n.emerald; t.values[4] = n.taken;
        t.values[5] = g.run.node_on_map && x == g.run.node_x && y == g.run.node_y;
        nodes[y * 7 + x] = w.add(t);
    }
    Token boss = option(Control::Boss, 0, static_cast<int>(g.run.boss_key) + 1);
    nodes[105] = w.add(boss);
    List<uint8_t, 8> choices{}; map_choices(g, choices);
    for (int i = 0; i < choices.size; ++i) options[i] = nodes[choices[i]];
}
SPIRE_FN inline Result encode(const Game& g, History& history, View view, ActionSet& actions) {
    actions.clear();
    if (g.fault) { view.finish(0, 0); return {g.fault, 0, 0}; }
    Writer w{view};
    int hands[hand_limit]{}, monsters[monster_capacity]{}, potions[potion_capacity]{}, options[card_capacity]{};
    Token screen = token(Kind::Screen, static_cast<int>(g.screen) + 1);
    screen.values[0] = g.run.act; screen.values[1] = g.run.floor; screen.values[2] = g.run.ascension;
    screen.values[3] = g.run.ruby_key; screen.values[4] = g.run.emerald_key; screen.values[5] = g.run.sapphire_key;
    if (g.screen == Screen::Event) { screen.ids[5] = static_cast<int>(g.event.id) + 1; screen.values[6] = event_page(g); }
    if (g.screen == Screen::Room) screen.ids[5] = static_cast<int>(g.room_kind) + 1;
    if (g.screen == Screen::Grid || g.screen == Screen::Discard) {
        screen.values[7] = selection_prompt(g);
        screen.values[8] = g.screen == Screen::Grid ? (g.grid_confirm_screen || g.selection_confirmation ? 0 : g.grid_num_cards) : g.selection_amount;
        screen.values[9] = g.selected_cards.size;
        screen.values[10] = g.selection_any_number;
        screen.values[11] = g.screen == Screen::Discard && g.selection_can_pick_zero;
        screen.values[12] = g.screen == Screen::Grid && (g.grid_confirm_screen || g.selection_confirmation);
    }
    w.add(screen);
    const bool combat = g.room_phase == RoomPhase::Combat && g.screen != Screen::Death && g.screen != Screen::Victory;
    Token player = token(Kind::Player, 1);
    player.values[0] = g.player.creature.hp; player.values[1] = g.player.creature.max_hp; player.values[2] = g.run.gold;
    if (combat) { player.values[3] = g.player.creature.block; player.values[4] = g.player.energy; }
    const int player_id = w.add(player);
    for (int i = 0; i < g.run.master_deck.size; ++i) w.add(card_token(g, g.run.master_deck[i], Zone::Master, i + 1));
    for (int i = 0; i < g.run.relics.size; ++i) w.add(relic_token(g.run.relics[i], Zone::None, i + 1, true));
    for (int i = 0; i < g.run.potion_slots; ++i) {
        Token t = token(Kind::Potion, static_cast<int>(g.run.potions[i]) + 1, Zone::None, i + 1);
        if (g.run.potions[i] != PotionId::Empty) t.values[0] = potion_potency(g, i);
        potions[i] = w.add(t);
    }
    if (combat) {
        powers(w, g, g.player.creature, player_id);
        w.add(token(Kind::Stance, static_cast<int>(g.player.stance) + 1, Zone::None, 0, player_id));
        for (int i = 0; i < g.orbs.slots.size; ++i) {
            const int slot = g.orbs.slots[i];
            Token t = token(Kind::Orb, slot < 0 ? 0 : static_cast<int>(g.orbs.kind(slot)) + 1, Zone::None, i + 1, player_id);
            if (slot >= 0) { t.values[0] = g.orbs.objects[slot].passive; t.values[1] = g.orbs.objects[slot].evoke; }
            w.add(t);
        }
        int position = 0;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const auto& m = g.monsters[g.monster_order[i]];
            const auto& c = m.creature;
            if (c.dead || c.escaped) continue;
            Token t = token(Kind::Monster, static_cast<int>(m.id) + 1, Zone::None, ++position);
            t.values[0] = c.hp; t.values[2] = c.block;
            // AbstractCreature.renderHealthText shows Dead instead of HP/max HP at zero health.
            if (c.hp > 0) t.values[1] = c.max_hp;
            t.values[3] = static_cast<int>(c.draw_x); t.values[4] = c.flip_horizontal;
            const bool show_intent = g.screen == Screen::Combat && !c.dying && !c.half_dead &&
                !has_relic(g, RelicId::RunicDome) && m.intent != Intent::None;
            if (show_intent) {
                t.values[6] = static_cast<int>(m.intent) + 1;
                if (m.intent == Intent::Attack || m.intent == Intent::AttackBuff || m.intent == Intent::AttackDebuff || m.intent == Intent::AttackDefend) {
                    t.values[7] = m.intent_damage; t.values[8] = m.intent_multi ? m.intent_hits : 1;
                }
            }
            monsters[i] = w.add(t);
            if (!c.dying && !c.half_dead) powers(w, g, c, monsters[i]);
        }
        for (int i = 0; i < g.hand.size; ++i) hands[i] = w.add(card_token(g, g.cards[g.hand[i]], Zone::Hand, i + 1));
        const bool eye = has_relic(g, RelicId::FrozenEye);
        pile(w, g, g.draw, Zone::Draw, !eye);
        pile(w, g, g.discard, Zone::Discard, false);
        pile(w, g, g.exhaust, Zone::Exhaust, !eye);
    }
    switch (g.screen) {
    case Screen::Discard:
        for (int i = 0; i < g.hand.size; ++i) options[i] = hands[i];
        for (int i = 0; i < g.selected_cards.size; ++i) w.add(card_token(g, g.cards[g.selected_cards[i]], Zone::Selected, i + 1));
        break;
    case Screen::Grid: {
        const auto& grid = get_pile(g, g.grid_source);
        for (int i = 0; i < grid.size; ++i) {
            const Card& c = get_card(g, g.grid_source == PileId::MasterDeck ? -grid[i] - 2 : grid[i]);
            Token t = card_token(g, c, Zone::Grid, i + 1);
            for (int j = 0; j < g.selected_cards.size; ++j) if (g.selected_cards[j] == grid[i]) t.values[12] = 1;
            if (g.grid_confirm_screen && g.grid_hovered_card == grid[i]) t.values[13] = 1;
            options[i] = w.add(t);
        }
        break;
    }
    case Screen::CardReward:
        for (int i = 0; i < g.reward_cards.size; ++i) options[i] = w.add(card_token(g, g.cards[g.reward_cards[i]], Zone::Offer, i + 1));
        if (!g.room_in_combat && has_relic(g, RelicId::SingingBowl)) options[g.reward_cards.size] = w.add(option(Control::Bowl, g.reward_cards.size + 1));
        break;
    case Screen::CombatReward: {
        const int first = w.result.entities + 1;
        for (int i = 0; i < g.screen_rewards.size; ++i) {
            const auto& r = g.reward_objects[g.screen_rewards[i]];
            Token t = token(Kind::Reward, static_cast<int>(r.kind) + 1, Zone::Offer, i + 1);
            if (r.kind == RewardKind::Gold || r.kind == RewardKind::StolenGold) t.values[0] = r.amount + r.bonus_gold;
            if (r.kind == RewardKind::Relic) t.ids[5] = static_cast<int>(r.relic.id) + 1;
            if (r.kind == RewardKind::Potion) t.ids[5] = static_cast<int>(r.potion) + 1;
            for (int j = 0; j < g.screen_rewards.size; ++j)
                if (r.relic_link == g.reward_objects[g.screen_rewards[j]].identity) t.values[1] = first + j;
            options[i] = w.add(t);
        }
        break;
    }
    case Screen::BossReward:
        for (int i = 0; i < g.chest.relics.size; ++i) options[i] = w.add(relic_token(g.reward_objects[g.chest.relics[i]].relic, Zone::Offer, i + 1, false));
        break;
    case Screen::Map: map_entities(w, g, options); break;
    case Screen::Shop: shop_entities(w, g, options); break;
    case Screen::Event: event_entities(w, g, options); break;
    case Screen::Room:
        if (g.room_kind == RoomKind::Rest && !g.campfire.something_selected) {
            if (history.rest_floor != g.run.floor) {
                // RestOption's constructor freezes the label before later potion use.
                history.rest_floor = g.run.floor;
                history.rest_heal = static_cast<int>(g.player.creature.max_hp * 0.3f);
                history.rest_bonus = has_relic(g, RelicId::RegalPillow) ? 15 : 0;
            }
            int choice = 0;
            for (int i = 0; i < g.campfire.options.size; ++i) {
                const auto& c = g.campfire.options[i];
                Token t = option(Control::Campfire, i + 1, static_cast<int>(c.kind) + 1);
                t.values[0] = !c.usable;
                if (c.kind == CampfireOption::Rest && c.usable) {
                    t.values[1] = history.rest_heal;
                    t.values[2] = history.rest_bonus;
                }
                const int id = w.add(t);
                if (c.usable) options[choice++] = id;
            }
        }
        if (g.room_kind == RoomKind::Treasure || g.room_kind == RoomKind::BossTreasure) {
            Token t = option(Control::OpenChest, 1); t.values[0] = g.chest.open; t.values[1] = g.chest.size;
            options[0] = w.add(t);
        }
        if (g.room_kind == RoomKind::Shop) options[0] = w.add(option(Control::Merchant, 1));
        break;
    default: break;
    }
    legal_actions(g, actions);
    if (actions.size > candidate_capacity) w.result.fault = static_cast<uint32_t>(Fault::Candidates);
    if (w.result.fault) { view.finish(0, 0, w.result.entities); actions.clear(); return {w.result.fault, 0, 0}; }
    for (int i = 0; i < actions.size; ++i) {
        const auto& a = actions[i];
        view.candidate_kinds[i] = w.byte(static_cast<int>(a.kind) + 1);
        const int subject = a.kind == ActionKind::Play ? hands[a.index] :
            a.kind == ActionKind::PotionUse || a.kind == ActionKind::PotionDiscard ? potions[a.index] : 0;
        view.candidate_refs[i] = w.word(subject);
        view.candidate_refs[candidate_capacity + i] = w.word(a.target >= 0 ? monsters[a.target] : 0);
        view.candidate_refs[2 * candidate_capacity + i] = w.word(a.kind == ActionKind::Choose ? options[a.index] : 0);
    }
    if (w.result.fault) { view.finish(0, 0, w.result.entities, actions.size); actions.clear(); return {w.result.fault, 0, 0}; }
    view.finish(w.result.entities, actions.size);
    w.result.candidates = actions.size;
    return w.result;
}
}
