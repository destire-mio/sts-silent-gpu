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

static Game combat() {
    Game g{};
    reset(g, 42);
    CombatSetup setup{};
    setup.encounter = EncounterId::TwoFungiBeasts;
    for (int i = 0; i < 3; ++i) setup.deck.push_back(make_card(CardId::Strike_G));
    for (int i = 0; i < 2; ++i) setup.deck.push_back(make_card(CardId::Defend_G));
    start_combat(g, setup);
    return g;
}

static int hand_card(const Game& g, CardId id) {
    for (int i = 0; i < g.hand.size; ++i) if (g.cards[g.hand[i]].id == id) return i;
    std::exit(1);
}

static void test_vigor_and_anger() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::Vigor, 4);
    apply_power(g, 0, 0, PowerId::Anger, 3);
    const int hp = g.monsters[0].creature.hp;
    step(g, {ActionKind::Play, hand_card(g, CardId::Defend_G), -1});
    require(has_power(g.player.creature, PowerId::Vigor) && power_amount(g.monsters[0].creature, PowerId::Strength) == 3,
            "a skill retains Vigor and gives Anger Strength");
    step(g, {ActionKind::Play, hand_card(g, CardId::Strike_G), 0});
    require(g.monsters[0].creature.hp == hp - 10 && !has_power(g.player.creature, PowerId::Vigor),
            "the first attack receives Vigor before its queued removal");
    step(g, {ActionKind::Play, hand_card(g, CardId::Strike_G), 0});
    require(g.monsters[0].creature.hp == hp - 16 && power_amount(g.monsters[0].creature, PowerId::Strength) == 3,
            "the second attack uses base damage and does not trigger Anger");
}

static void test_shifting_and_artifact() {
    Game g = combat();
    apply_power(g, 0, 0, PowerId::Shifting, 5);
    apply_power(g, 0, 0, PowerId::Strength, 20);
    apply_power(g, 1, 1, PowerId::Shifting, 5);
    apply_power(g, 1, 1, PowerId::Strength, 20);
    apply_power(g, 1, 1, PowerId::Artifact, 1);
    DamageInfo hit{};
    hit.source = no_target;
    hit.target = 0;
    hit.amount = 7;
    hit.damage_type = DamageType::Thorns;
    damage(g, hit);
    require(g.manager.actions.size == 2 && g.manager.actions[0].power == PowerId::GainStrength &&
            g.manager.actions[1].power == PowerId::Strength, "Shifting queues its Strength restoration before its loss");
    run_until_decision(g);
    hit.target = 1;
    damage(g, hit);
    run_until_decision(g);
    require(power_amount(g.monsters[0].creature, PowerId::Strength) == 13 &&
            power_amount(g.monsters[0].creature, PowerId::GainStrength) == 7 &&
            power_amount(g.monsters[0].creature, PowerId::Shifting) == -1,
            "Shifting applies to source-less Thorns and keeps its sentinel");
    require(power_amount(g.monsters[1].creature, PowerId::Strength) == 20 &&
            !has_power(g.monsters[1].creature, PowerId::GainStrength) &&
            !has_power(g.monsters[1].creature, PowerId::Artifact), "Artifact prevents loss without adding restoration");
    at_end_of_turn(g, 0);
    run_until_decision(g);
    require(power_amount(g.monsters[0].creature, PowerId::Strength) == 20 &&
            !has_power(g.monsters[0].creature, PowerId::GainStrength) &&
            power_amount(g.monsters[1].creature, PowerId::Strength) == 20, "turn end restores only the affected monster");
}

static void test_sadistic() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::Sadistic, 5);
    apply_power(g, player_target, player_target, PowerId::Strength, 30);
    const int first_hp = g.monsters[0].creature.hp;
    const int second_hp = g.monsters[1].creature.hp;
    apply_power(g, player_target, 1, PowerId::Artifact, 1);
    apply_power(g, player_target, 0, PowerId::Weak, 2);
    apply_power(g, player_target, 1, PowerId::Weak, 2);
    apply_power(g, player_target, 0, PowerId::GainStrength, 3);
    apply_power(g, 0, 0, PowerId::Vulnerable, 1);
    apply_power(g, player_target, player_target, PowerId::Weak, 1);
    run_until_decision(g);
    require(g.monsters[0].creature.hp == first_hp - 5 && g.monsters[1].creature.hp == second_hp,
            "Sadistic deals raw Thorns only for its owner's unblocked enemy debuff, excluding Shackled");
    require(!has_power(g.monsters[1].creature, PowerId::Weak) && !has_power(g.monsters[1].creature, PowerId::Artifact),
            "Artifact blocks the debuff without triggering Sadistic");
}

static void test_temporary_stats() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::Strength, 10);
    apply_power(g, player_target, player_target, PowerId::Dexterity, 8);
    apply_power(g, player_target, player_target, PowerId::LoseStrength, 3);
    apply_power(g, player_target, player_target, PowerId::LoseDexterity, 4);
    apply_power(g, player_target, player_target, PowerId::WraithForm, -1);
    apply_power(g, player_target, player_target, PowerId::WraithForm, -1);
    at_end_of_turn(g, player_target);
    run_until_decision(g);
    require(power_amount(g.player.creature, PowerId::Strength) == 7 &&
            power_amount(g.player.creature, PowerId::Dexterity) == 2 &&
            !has_power(g.player.creature, PowerId::LoseStrength) &&
            !has_power(g.player.creature, PowerId::LoseDexterity), "temporary stat powers apply their losses and remove themselves");
    require(power_amount(g.player.creature, PowerId::WraithForm) == -2,
            "WraithForm stacks from minus one without the base sentinel restriction");
    at_end_of_turn(g, player_target);
    run_until_decision(g);
    require(!has_power(g.player.creature, PowerId::Dexterity) && power_amount(g.player.creature, PowerId::Strength) == 7,
            "WraithForm persists for another turn while temporary losses do not repeat");
}

static void test_gain_strength_and_generic() {
    Game g = combat();
    apply_power(g, 0, 0, PowerId::GainStrength, -2000);
    require(power_amount(g.monsters[0].creature, PowerId::GainStrength) == -999, "Shackled construction clamps at minus 999");
    apply_power(g, 0, 0, PowerId::GainStrength, 2000);
    require(power_amount(g.monsters[0].creature, PowerId::GainStrength) == 999, "Shackled stacks the raw incoming amount");
    apply_power(g, 0, 0, PowerId::GainStrength, -999);
    run_until_decision(g);
    require(!has_power(g.monsters[0].creature, PowerId::GainStrength), "zero Shackled stack removes it");
    apply_power(g, 0, 0, PowerId::GainStrength, -1);
    apply_power(g, 0, 0, PowerId::GainStrength, 4);
    Power& p = g.monsters[0].creature.powers[find_power(g.monsters[0].creature, PowerId::GainStrength)];
    power_reducePower(g, p, 0, 3);
    run_until_decision(g);
    require(!has_power(g.monsters[0].creature, PowerId::GainStrength), "Shackled reduction to zero queues removal");
    apply_power(g, 0, 0, PowerId::GenericStrengthUp, 3);
    at_end_of_round(g, 0);
    run_until_decision(g);
    at_end_of_round(g, 0);
    run_until_decision(g);
    require(power_amount(g.monsters[0].creature, PowerId::Strength) == 6 &&
            !has_power(g.monsters[1].creature, PowerId::Strength), "GenericStrengthUp increases its owner's Strength each round");
}

int main() {
    test_vigor_and_anger();
    test_shifting_and_artifact();
    test_sadistic();
    test_temporary_stats();
    test_gain_strength_and_generic();
    std::puts("Power 04 Java-derived cases passed (not oracle trace acceptance)");
}
