#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Power mismatch: %s\n", message);
        std::exit(1);
    }
}

static Game combat(CardId card = CardId::Defend_G) {
    Game g{};
    reset(g, 42);
    CombatSetup setup{};
    for (int i = 0; i < 12; ++i) setup.deck.push_back(make_card(card));
    start_combat(g, setup);
    return g;
}

static void test_constructor_state() {
    Game g = combat();
    queue_power(g, 0, player_target, PowerId::Poison, 12000);
    require(g.manager.actions[0].prepared_power.amount == 9999 &&
            g.manager.actions[0].prepared_power.state.Poison.source == 0,
            "Poison retains the source and clamps the constructor amount");
    run_until_decision(g);
    apply_power(g, player_target, player_target, PowerId::Poison, 2);
    const Power& poison = g.player.creature.powers[find_power(g.player.creature, PowerId::Poison)];
    require(poison.amount == 10001 && poison.state.Poison.source == 0,
            "Poison stacking uses the raw amount and keeps the first source");
    apply_power(g, player_target, player_target, PowerId::Panache, 10);
    apply_power(g, player_target, player_target, PowerId::Panache, 14);
    Power& panache = g.player.creature.powers[find_power(g.player.creature, PowerId::Panache)];
    require(panache.amount == 5 && panache.state.Panache.damage == 24,
            "Panache stacks damage without stacking its card countdown");
    step(g, {ActionKind::Play, 0, no_target});
    require(panache.amount == 4, "a skill advances Panache's countdown");
    power_atStartOfTurn(g, panache, player_target);
    require(panache.amount == 5, "Panache resets its countdown at turn start");
    apply_power(g, player_target, player_target, PowerId::PlatedArmor, 1500);
    require(power_amount(g.player.creature, PowerId::PlatedArmor) == 1500,
            "Plated Armor constructor does not apply its stack cap");
    apply_power(g, player_target, player_target, PowerId::PlatedArmor, 1);
    require(power_amount(g.player.creature, PowerId::PlatedArmor) == 999, "Plated Armor caps when stacking");
    apply_power(g, player_target, player_target, PowerId::ThousandCuts, -1);
    apply_power(g, player_target, player_target, PowerId::ThousandCuts, 3);
    require(power_amount(g.player.creature, PowerId::ThousandCuts) == 2,
            "Thousand Cuts stacks even when the previous amount is minus one");
}

static void test_pen_nib_and_sharp_hide() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::Strength, 3);
    apply_power(g, player_target, player_target, PowerId::PenNib, 1);
    apply_power(g, 0, 0, PowerId::SharpHide, 4);
    step(g, {ActionKind::Play, 0, no_target});
    require(has_power(g.player.creature, PowerId::PenNib) && g.player.creature.hp == 70,
            "playing a skill keeps Pen Nib and does not trigger Sharp Hide");
    const int card = g.cards.size;
    g.cards.push_back(make_card(CardId::Strike_G));
    g.hand.push_back(card);
    const int hp = g.monsters[0].creature.hp;
    step(g, {ActionKind::Play, g.hand.size - 1, 0});
    require(g.monsters[0].creature.hp == hp - 18 && !has_power(g.player.creature, PowerId::PenNib),
            "Pen Nib doubles damage after Strength and is removed after the attack");
    require(g.player.creature.hp == 70 && g.player.creature.block == 1,
            "Sharp Hide's attack trigger deals raw thorns damage against existing block");
}

static void test_plated_armor_and_thorns() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::PlatedArmor, 2);
    apply_power(g, player_target, player_target, PowerId::Thorns, 3);
    apply_power(g, 0, 0, PowerId::PlatedArmor, 5);
    g.player.creature.block = 10;
    const int hp = g.monsters[0].creature.hp;
    DamageInfo hit{};
    hit.source = 0;
    hit.target = player_target;
    hit.amount = 4;
    damage(g, hit);
    run_until_decision(g);
    require(g.monsters[0].creature.hp == hp - 3 && power_amount(g.player.creature, PowerId::PlatedArmor) == 2,
            "a blocked attack triggers Thorns without reducing Plated Armor");
    g.player.creature.block = 0;
    damage(g, hit);
    run_until_decision(g);
    require(g.player.creature.hp == 66 && g.monsters[0].creature.hp == hp - 6 &&
            power_amount(g.player.creature, PowerId::PlatedArmor) == 1 &&
            power_amount(g.monsters[0].creature, PowerId::PlatedArmor) == 5,
            "HP damage reduces only the damaged creature's armor; retaliation does not reduce armor");
    hit.damage_type = DamageType::HpLoss;
    damage(g, hit);
    run_until_decision(g);
    require(g.monsters[0].creature.hp == hp - 6 && power_amount(g.player.creature, PowerId::PlatedArmor) == 1,
            "HP loss triggers neither Thorns nor armor reduction");
    at_end_of_turn(g, 0);
    run_until_decision(g);
    require(g.monsters[0].creature.block == 5, "monster Plated Armor gains block at its end turn");
}

static void test_slow() {
    Game g = combat(CardId::Strike_G);
    apply_power(g, 0, 0, PowerId::Slow, 0);
    const int hp = g.monsters[0].creature.hp;
    step(g, {ActionKind::Play, 0, 0});
    require(g.monsters[0].creature.hp == hp - 6 && power_amount(g.monsters[0].creature, PowerId::Slow) == 1,
            "Slow increments after the first card's damage");
    step(g, {ActionKind::Play, 0, 0});
    step(g, {ActionKind::Play, 0, 0});
    require(g.monsters[0].creature.hp == hp - 19 && power_amount(g.monsters[0].creature, PowerId::Slow) == 3,
            "subsequent attacks use the preceding Slow count and Java float arithmetic");
    at_end_of_round(g, 0);
    require(power_amount(g.monsters[0].creature, PowerId::Slow) == 0, "Slow clears at round end");
}

static void test_reactive_rng() {
    Game g = combat();
    apply_power(g, 0, 0, PowerId::Reactive, 1);
    const int counter = g.rng.ai.counter;
    DamageInfo hit{};
    hit.source = player_target;
    hit.target = 0;
    hit.amount = 1;
    damage(g, hit);
    require(g.rng.ai.counter == counter, "Reactive defers the AI roll until the queued action");
    run_until_decision(g);
    require(g.rng.ai.counter == counter + 1, "a nonlethal normal hit rerolls with one AI stream call");
    hit.damage_type = DamageType::Thorns;
    damage(g, hit);
    run_until_decision(g);
    require(g.rng.ai.counter == counter + 1, "thorns damage does not reroll the move");
    hit.damage_type = DamageType::Normal;
    hit.amount = g.monsters[0].creature.hp;
    damage(g, hit);
    require(g.rng.ai.counter == counter + 1 && g.manager.actions.empty(), "a lethal hit does not reroll");
}

static void test_bomb_identity() {
    Game g = combat();
    Game other = combat();
    queue_power(g, player_target, player_target, PowerId::TheBomb, 1, false, 40);
    queue_power(g, player_target, player_target, PowerId::TheBomb, 3, false, 50);
    require(g.bomb_id_offset == 2 && other.bomb_id_offset == 0,
            "bomb IDs allocate during construction and belong to each game");
    run_until_decision(g);
    require(g.player.creature.powers.size == 2 && g.player.creature.powers[0].id_offset == 0 &&
            g.player.creature.powers[1].id_offset == 1, "separate bombs do not stack");
    at_end_of_turn(g, player_target);
    run_until_decision(g);
    require(g.player.creature.powers.size == 1 && g.player.creature.powers[0].id_offset == 1 &&
            g.player.creature.powers[0].amount == 2 && g.player.creature.powers[0].state.TheBomb.damage == 50,
            "removing the first bomb does not redirect the second bomb's queued reduction");
}

static void test_stasis_discard_identity() {
    Game g = combat();
    while (g.hand.size < 10) g.hand.push_back(g.draw.pop_back());
    const int captured = g.draw.pop_back();
    apply_power(g, 0, 0, PowerId::Stasis, 0, false, 0, captured);
    g.monsters[0].creature.hp = 0;
    monster_die(g, 0);
    run_until_decision(g);
    const Card& returned = g.cards[g.discard[g.discard.size - 1]];
    require(returned.uuid == g.cards[captured].uuid && g.discard[g.discard.size - 1] != captured &&
            g.hand.size == 10, "Stasis returns a distinct card copy with the captured UUID to a full hand's discard pile");
}

static void test_tools_of_the_trade() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::ToolsOfTheTrade, 2);
    const int draw = g.draw.size;
    step(g, {ActionKind::EndTurn});
    require(g.screen == Screen::Discard && g.hand.size == 7 && g.draw.size == draw - 7 &&
            g.selection_amount == 2, "Tools draws two cards after the normal five-card draw, then opens discard selection");
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Proceed});
    require(g.screen == Screen::Combat && g.hand.size == 5 && g.manager.total_discarded_this_turn == 2,
            "Tools completes a manual two-card discard before returning to combat input");
}

int main() {
    test_constructor_state();
    test_pen_nib_and_sharp_hide();
    test_plated_armor_and_thorns();
    test_slow();
    test_reactive_rng();
    test_bomb_identity();
    test_stasis_discard_identity();
    test_tools_of_the_trade();
    std::puts("power batch checks passed");
}
