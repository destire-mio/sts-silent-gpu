#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;
using namespace spire::units::actions;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Action mismatch: %s\n", message);
        std::exit(1);
    }
}

static Game combat() {
    Game g{};
    reset(g, 42);
    CombatSetup setup{};
    for (int i = 0; i < 3; ++i) setup.deck.push_back(make_card(CardId::Defend_G));
    setup.deck.push_back(make_card(CardId::Strike_G));
    start_combat(g, setup);
    return g;
}

static void test_damage_and_x_cost() {
    Game g = combat();
    const int hp = g.monsters[0].creature.hp;
    apply_power(g, player_target, player_target, PowerId::Strength, 9);
    DamageInfo info{};
    info.source = player_target;
    info.amount = info.info_base = 5;
    add_bottom(g, FlechetteAction::construct(0, info));
    run_until_decision(g);
    require(g.monsters[0].creature.hp == hp - 15, "Flechette uses calculated damage once per skill");
    require(g.hand.size == 4 && g.player.energy == 3, "Flechette preserves hand and energy");
    add_bottom(g, FlechetteAction::construct(no_target, info));
    run_until_decision(g);
    require(g.monsters[0].creature.hp == hp - 15, "Flechette null-target hits cancel without changing another creature");

    g = combat();
    const int skewer_hp = g.monsters[0].creature.hp;
    add_bottom(g, SkewerAction::construct(player_target, 0, 4, DamageType::Normal, false, 1));
    run_until_decision(g);
    require(g.monsters[0].creature.hp == skewer_hp - 4 && g.player.energy == 0,
            "Skewer uses energyOnUse for hits and consumes current energy");
    add_bottom(g, MalaiseAction::construct(player_target, 0, true, true, 1));
    run_until_decision(g);
    require(power_amount(g, g.monsters[0].creature, PowerId::Strength) == -2 &&
            power_amount(g, g.monsters[0].creature, PowerId::Weak) == 2,
            "Malaise applies Strength and Weak with its upgrade bonus");
    g.player.energy = 3;
    add_bottom(g, SkewerAction::construct(player_target, 0, 4, DamageType::Normal, false, 0));
    run_until_decision(g);
    require(g.player.energy == 3, "zero-effect X action does not consume energy");
}

static void test_debuff_removal() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::Strength, 3);
    apply_power(g, player_target, player_target, PowerId::Dexterity, -2);
    apply_power(g, player_target, player_target, PowerId::Weak, 2);
    Effect remove{};
    remove.kind = EffectKind::RemoveDebuffsAction;
    add_bottom(g, remove);
    run_until_decision(g);
    require(power_amount(g, g.player.creature, PowerId::Strength) == 3 &&
            !has_power(g, g.player.creature, PowerId::Dexterity) && !has_power(g, g.player.creature, PowerId::Weak),
            "RemoveDebuffs removes negative-stat powers and preserves positive buffs");
}

static void test_draw_and_block() {
    Game g = combat();
    Effect conditional = ConditionalDrawAction::construct(1, CardType::Attack);
    execute_effect(g, conditional);
    require(g.manager.actions.empty(), "ConditionalDraw suppresses draw for a restricted hand card");
    int strike = -1;
    for (int i = 0; i < g.hand.size; ++i)
        if (g.cards[g.hand[i]].id == CardId::Strike_G) { strike = g.hand.erase(i); break; }
    g.draw.push_back(strike);
    add_bottom(g, ConditionalDrawAction::construct(1, CardType::Attack));
    run_until_decision(g);
    require(g.hand.size == 4 && g.hand[3] == strike, "ConditionalDraw draws when the restricted type is absent");
    g.drawn_cards.clear();
    g.drawn_cards.push_back(strike);
    Effect escape{};
    escape.kind = EffectKind::EscapePlanAction;
    escape.amount = 7;
    add_bottom(g, escape);
    run_until_decision(g);
    require(g.player.creature.block == 0, "EscapePlan ignores an attack draw");
    g.drawn_cards.push_back(0);
    add_bottom(g, escape);
    run_until_decision(g);
    require(g.player.creature.block == 7, "EscapePlan grants block once when a skill was drawn");
}

static void test_gambling_selection() {
    Game g = combat();
    Effect gamble{};
    gamble.kind = EffectKind::GamblingChipAction;
    add_bottom(g, gamble);
    run_until_decision(g);
    ActionSet legal{};
    legal_actions(g, legal);
    require(legal.size == 5 && legal[4].kind == ActionKind::Proceed, "GamblingChip allows zero selected cards");
    const int first = g.hand[0];
    const int second = g.hand[1];
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Proceed});
    require(g.hand.size == 4 && g.manager.total_discarded_this_turn == 2 && g.selected_cards_retrieved,
            "GamblingChip replaces the selected number of cards");
    require(g.selected_cards[0] == first && g.selected_cards[1] == second,
            "GamblingChip preserves selected-card identities after retrieval");

    g = combat();
    add_bottom(g, gamble);
    run_until_decision(g);
    step(g, {ActionKind::Proceed});
    require(g.hand.size == 4 && g.manager.total_discarded_this_turn == 0 && g.rng.card_random.counter == 0,
            "GamblingChip zero selection leaves cards and RNG untouched");
}

static void test_calculated_gamble() {
    Game g = combat();
    Effect gamble{};
    gamble.kind = EffectKind::CalculatedGambleAction;
    add_bottom(g, gamble);
    run_until_decision(g);
    require(g.hand.size == 4 && g.manager.total_discarded_this_turn == 4 && g.cards.size == 4,
            "CalculatedGamble discards before drawing and reuses the original cards");
    require(g.rng.card_random.counter == 0, "full-hand CalculatedGamble discards without random-card draws");
}

static void test_stasis_selection() {
    Game g = combat();
    const int slimed = g.cards.size;
    g.cards.push_back(make_card(CardId::Slimed));
    const int dazed = g.cards.size;
    g.cards.push_back(make_card(CardId::Dazed));
    g.draw.push_back(slimed);
    g.draw.push_back(dazed);
    Random expected = g.rng.card_random;
    const int selected = expected.random(1) == 0 ? dazed : slimed;
    Effect stasis{};
    stasis.kind = EffectKind::ApplyStasisAction;
    stasis.target = 0;
    add_bottom(g, stasis);
    run_until_decision(g);
    require(g.limbo.empty() && g.draw.size == 1 &&
            g.monsters[0].creature.powers.at(g.power_objects, find_power(g, g.monsters[0].creature, PowerId::Stasis)).state.Stasis.card == selected,
            "ApplyStasis sorts the available highest-rarity cards by Java ID before selection");
    require(g.rng.card_random.counter == expected.counter &&
            g.rng.card_random.generator.s0 == expected.generator.s0 &&
            g.rng.card_random.generator.s1 == expected.generator.s1,
            "ApplyStasis consumes one cardRandomRng call and none for absent rarities");
}

static void test_use_card_movement() {
    Game g = combat();
    Card& card = g.cards[0];
    card.return_to_hand = true;
    card.free_to_play_once = true;
    card.in_autoplay = true;
    card.dont_trigger_on_use_card = true;
    card.cost_for_turn = 0;
    Effect use = UseCard::construct(g, 0, no_target);
    for (int i = 0; i < g.hand.size; ++i) if (g.hand[i] == 0) { g.hand.erase(i); break; }
    g.player.card_in_use = 0;
    g.manager.card_queue.push_back({0, no_target, 3});
    g.manager.card_queue.push_back({1, no_target, 3});
    execute_effect(g, use);
    require(g.hand.size == 4 && g.hand[3] == 0 && g.discard.empty() && g.exhaust.empty(),
            "UseCard returns the same instance to hand");
    require(card.cost_for_turn == 0 && !card.free_to_play_once && !card.in_autoplay &&
            !card.dont_trigger_on_use_card && g.player.card_in_use == -1,
            "UseCard resets play flags and preserves the returned card's turn cost");
    require(g.manager.card_queue.size == 1 && g.manager.card_queue[0].card == 1,
            "card movement removes its queued instance without removing another card");

    g = combat();
    g.cards[0].exhaust_on_use_once = true;
    use = UseCard::construct(g, 0, no_target);
    for (int i = 0; i < g.hand.size; ++i) if (g.hand[i] == 0) { g.hand.erase(i); break; }
    execute_effect(g, use);
    require(g.exhaust.size == 1 && g.exhaust[0] == 0 && !g.cards[0].exhaust_on_use_once,
            "UseCard honors exhaustOnUseOnce and clears it after exhaustion");
}

static void test_summon_construction() {
    Game g = combat();
    int8_t slots[]{-1, -1, -1};
    const int ai_calls = g.rng.ai.counter;
    const int hp_calls = g.rng.monster_hp.counter;
    Effect summon = SummonGremlinAction::construct(g, slots);
    require(slots[0] == 1 && slots[1] == -1 && g.monster_order.size == 1,
            "SummonGremlin replaces the first empty owner slot before entering the room list");
    require(g.rng.ai.counter == ai_calls + 1 && g.rng.monster_hp.counter == hp_calls + 1,
            "SummonGremlin chooses its type and HP during construction");
    add_bottom(g, summon);
    run_until_decision(g);
    require(g.monster_order.size == 2 && g.monster_order[0] == slots[0] && g.monster_order[1] == 0 &&
            g.rng.ai.counter == ai_calls + 2,
            "SummonGremlin rolls its move and inserts the new identity in drawX order");
}

int main() {
    test_damage_and_x_cost();
    test_debuff_removal();
    test_draw_and_block();
    test_gambling_selection();
    test_calculated_gamble();
    test_stasis_selection();
    test_use_card_movement();
    test_summon_construction();
    std::puts("action:05 behavior checks passed");
}
