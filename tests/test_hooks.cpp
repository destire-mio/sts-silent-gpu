#include "core/engine.hpp"
#include "core/lifecycle.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Hook mismatch: %s\n", message);
        std::exit(1);
    }
}

static Game combat() {
    Game g{};
    reset(g, 0);
    CombatSetup setup{};
    for (int i = 0; i < 5; ++i) setup.deck.push_back(make_card(CardId::Defend_G));
    start_combat(g, setup);
    return g;
}

static void test_constructor_and_stack() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::NoDraw, 5, true);
    apply_power(g, player_target, player_target, PowerId::Artifact, 1);
    apply_power(g, player_target, player_target, PowerId::NoDraw, 3);
    require(power_amount(g, g.player.creature, PowerId::NoDraw) == -1, "NoDraw constructor sets sentinel amount");
    require(power_amount(g, g.player.creature, PowerId::Artifact) == 1 && g.manager.actions.empty(),
            "duplicate NoDraw returns before Artifact consumes a charge");
    apply_power(g, player_target, player_target, PowerId::Strength, -900);
    run_until_decision(g);
    require(!has_power(g, g.player.creature, PowerId::Strength) && !has_power(g, g.player.creature, PowerId::Artifact),
            "negative Strength is a debuff at initial application");
    apply_power(g, player_target, player_target, PowerId::Strength, -900);
    apply_power(g, player_target, player_target, PowerId::Strength, 2000);
    require(power_amount(g, g.player.creature, PowerId::Strength) == 999,
            "ApplyPowerAction stacks the raw amount rather than the constructor-clamped amount");
    apply_power(g, player_target, player_target, PowerId::Strength, -999);
    run_until_decision(g);
    require(!has_power(g, g.player.creature, PowerId::Strength), "zero Strength stack removes the power");
    apply_power(g, player_target, player_target, PowerId::Artifact, 0);
    apply_power(g, player_target, player_target, PowerId::Weak, 1);
    run_until_decision(g);
    require(!has_power(g, g.player.creature, PowerId::Artifact) && !has_power(g, g.player.creature, PowerId::Weak),
            "zero Artifact blocks a debuff and then removes itself");
    step(g, {ActionKind::EndTurn});
    require(!has_power(g, g.player.creature, PowerId::NoDraw) && g.hand.size == 5,
            "NoDraw end-turn removal precedes next-turn draw");
}

static void test_power_preparation() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::Strength, -900);
    queue_power(g, player_target, player_target, PowerId::Strength, 2000);
    require(g.action_objects[g.manager.actions[0]].amount == 2000 && g.action_objects[g.manager.actions[0]].payload.power.amount == 999,
            "Power construction precedes queue resolution and preserves the raw stack amount");
    Effect no_draw{};
    no_draw.kind = EffectKind::ApplyPower;
    no_draw.power = PowerId::NoDraw;
    no_draw.amount = 7;
    add_top(g, no_draw);
    require(g.action_objects[g.manager.actions[0]].payload.power.amount == -1,
            "direct ApplyPower enqueue runs the same constructor preparation");
    run_until_decision(g);
    require(power_amount(g, g.player.creature, PowerId::Strength) == 999 &&
            power_amount(g, g.player.creature, PowerId::NoDraw) == -1,
            "prepared power execution uses the constructor object and raw stack amount");
}

static void test_constructor_time() {
    Game g = combat();
    queue_power(g, 0, player_target, PowerId::Weak, 3, true);
    queue_power(g, 0, player_target, PowerId::Vulnerable, 2, true);
    queue_power(g, 0, player_target, PowerId::Frail, 3, true);
    g.manager.turn_has_ended = true;
    while (!g.manager.actions.empty()) {
        Effect next = g.action_objects[g.manager.actions.pop_front()];
        execute_effect(g, next);
    }
    g.manager.turn_has_ended = false;
    at_end_of_round(g, player_target);
    run_until_decision(g);
    require(power_amount(g, g.player.creature, PowerId::Weak) == 3 &&
            power_amount(g, g.player.creature, PowerId::Frail) == 3 &&
            power_amount(g, g.player.creature, PowerId::Vulnerable) == 1,
            "Vulnerable constructor sees the pre-queue turn flag; Weak and Frail skip the first round");
    at_end_of_round(g, player_target);
    run_until_decision(g);
    require(power_amount(g, g.player.creature, PowerId::Weak) == 2 &&
            power_amount(g, g.player.creature, PowerId::Frail) == 2 &&
            !has_power(g, g.player.creature, PowerId::Vulnerable), "duration hooks run in later rounds");
}

static void test_attacked_hook_order() {
    Game g = combat();
    apply_power(g, 0, 0, PowerId::Angry, 2);
    apply_power(g, 0, 0, PowerId::CurlUp, 7);
    DamageInfo hit{};
    hit.source = player_target;
    hit.target = 0;
    hit.amount = 3;
    hit.damage_type = DamageType::Thorns;
    damage(g, hit);
    require(g.manager.actions.empty(), "Thorns does not trigger Angry or Curl Up");
    hit.damage_type = DamageType::Normal;
    g.monsters[0].creature.block = 6;
    damage(g, hit);
    require(g.manager.actions.empty(), "fully blocked normal damage does not trigger Angry or Curl Up");
    hit.amount = 9;
    damage(g, hit);
    require(g.manager.actions.size == 4 && g.action_objects[g.manager.actions[0]].kind == EffectKind::ApplyPower &&
            g.action_objects[g.manager.actions[0]].power == PowerId::Strength && g.action_objects[g.manager.actions[1]].kind == EffectKind::ChangeState && g.action_objects[g.manager.actions[2]].kind == EffectKind::Block && g.action_objects[g.manager.actions[3]].kind == EffectKind::RemovePower,
            "Angry adds Strength before Curl Up gains block and removes itself");
    run_until_decision(g);
    require(g.monsters[0].creature.hp == 47 && g.monsters[0].creature.block == 7 &&
            power_amount(g, g.monsters[0].creature, PowerId::Strength) == 2 &&
            !has_power(g, g.monsters[0].creature, PowerId::CurlUp), "attacked hooks resolve before the next decision");
}

static void test_lifecycle() {
    Game g = combat();
    g.run.gold = 3;
    lose_gold(g, 20);
    require(g.run.gold == 0, "gold loss caps at the actual balance");
    gain_gold(g, -5);
    require(g.run.gold == 0, "negative gold gain leaves balance unchanged");
    gain_gold(g, 9);
    spend_gold(g, 2);
    require(g.run.gold == 7, "gold services update the run balance");
    equip_relic(g, RelicId::SnakeRing);
    require(g.run.relics.size == 1 && g.run.relics[0].counter == -1, "relic acquisition stores constructor counter");
    require(unequip_relic(g, RelicId::SnakeRing) && g.run.relics.empty(), "relic removal erases the acquired relic");
    require(!unequip_relic(g, RelicId::SnakeRing), "missing relic removal returns false");
    Card acquired = make_card(CardId::Neutralize, 1);
    preview_card_obtain(g, acquired);
    obtain_card(g, acquired);
    require(g.run.master_deck.size == 6 && g.run.master_deck[5].id == CardId::Neutralize &&
            g.run.master_deck[5].upgrades == 1, "obtained card retains its upgraded state at the deck top");
    remove_master_card(g, 0);
    require(g.run.master_deck.size == 5 && g.run.master_deck[4].id == CardId::Neutralize,
            "master deck removal preserves remaining order");
    g.player.energy = 998;
    const int previous_gain = g.manager.energy_gained_this_combat;
    gain_energy(g, 5);
    require(g.player.energy == 999 && g.manager.energy_gained_this_combat == previous_gain,
            "AbstractPlayer.gainEnergy caps energy; GainEnergyAction owns the combat counter");
    gain_energy(g, -1000);
    require(g.player.energy == -1, "EnergyPanel.addEnergy does not clamp negative energy");
}

int main() {
    test_constructor_and_stack();
    test_power_preparation();
    test_constructor_time();
    test_attacked_hook_order();
    test_lifecycle();
    std::puts("Java-derived hook cases passed (not oracle trace acceptance)");
}
