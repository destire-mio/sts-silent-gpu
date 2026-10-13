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
    reset(g, 101);
    CombatSetup setup{};
    for (int i = 0; i < 10; ++i) setup.deck.push_back(make_card(CardId::Defend_G));
    start_combat(g, setup);
    return g;
}

static void resolve_actions(Game& g) {
    while (!g.manager.actions.empty()) {
        Effect e = g.action_objects[g.manager.actions.pop_front()];
        execute_effect(g, e);
    }
}

static void test_constructors_and_stacking() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::Accuracy, -1);
    apply_power(g, player_target, player_target, PowerId::Accuracy, 5);
    require(power_amount(g, g.player.creature, PowerId::Accuracy) == 4, "Accuracy stacks the sentinel value");
    apply_power(g, player_target, player_target, PowerId::Buffer, -1);
    apply_power(g, player_target, player_target, PowerId::Buffer, 5);
    require(power_amount(g, g.player.creature, PowerId::Buffer) == 4, "Buffer stacks the sentinel value");
    apply_power(g, player_target, 0, PowerId::CorpseExplosion, 99);
    apply_power(g, player_target, 0, PowerId::CorpseExplosion, 2);
    require(power_amount(g, g.monsters[0].creature, PowerId::CorpseExplosion) == 3,
            "Corpse Explosion constructs one stack, then uses the raw stack input");
    apply_power(g, player_target, player_target, PowerId::Energized, 2000);
    require(power_amount(g, g.player.creature, PowerId::Energized) == 999, "Energized constructor cap");
    apply_power(g, player_target, player_target, PowerId::Energized, -1000);
    apply_power(g, player_target, player_target, PowerId::Energized, 5);
    require(power_amount(g, g.player.creature, PowerId::Energized) == -1, "Energized inherits sentinel stacking");
    apply_power(g, player_target, player_target, PowerId::Dexterity, -2000);
    apply_power(g, player_target, player_target, PowerId::Dexterity, 2000);
    require(power_amount(g, g.player.creature, PowerId::Dexterity) == 999, "Dexterity stacks raw input after construction");
    apply_power(g, player_target, player_target, PowerId::Dexterity, -999);
    resolve_actions(g);
    require(!has_power(g, g.player.creature, PowerId::Dexterity), "zero Dexterity removes itself");
}

static void test_confusion_rng() {
    Game g = combat();
    apply_power(g, 0, player_target, PowerId::Confusion, 10);
    Power& p = g.player.creature.powers.at(g.power_objects, find_power(g, g.player.creature, PowerId::Confusion));
    const int card = g.hand[0];
    Random expected = g.rng.card_random;
    const int new_cost = expected.random(3);
    const int other_counter = g.rng.misc.counter;
    g.cards[card].cost_for_turn = 9;
    g.cards[card].free_to_play_once = true;
    power_onCardDraw(g, p, player_target, card);
    require(g.cards[card].cost == new_cost &&
            g.cards[card].cost_for_turn == (new_cost == 1 ? 9 : new_cost) &&
            !g.cards[card].free_to_play_once, "Confusion preserves turn cost when the rolled base cost matches");
    require(g.rng.card_random.counter == expected.counter && g.rng.misc.counter == other_counter,
            "Confusion consumes one cardRandomRng call");
    g.cards[card].cost = -1;
    g.cards[card].free_to_play_once = true;
    power_onCardDraw(g, p, player_target, card);
    require(g.rng.card_random.counter == expected.counter && g.cards[card].free_to_play_once,
            "negative-cost cards skip both the RNG draw and free-play reset");
}

static void test_damage_hooks() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::Buffer, 1);
    g.player.creature.block = 4;
    const int hp = g.player.creature.hp;
    queue_damage(g, 0, player_target, 4);
    resolve_actions(g);
    require(g.player.creature.hp == hp && has_power(g, g.player.creature, PowerId::Buffer),
            "a blocked hit does not consume Buffer");
    queue_damage(g, no_target, player_target, 3, DamageType::HpLoss);
    resolve_actions(g);
    require(g.player.creature.hp == hp && !has_power(g, g.player.creature, PowerId::Buffer),
            "Buffer prevents HP loss and removes itself through the queued reduction");
    apply_power(g, player_target, player_target, PowerId::DoubleDamage, 2, true);
    require(calculate_damage(g, player_target, 0, 7) == 14 &&
            calculate_damage(g, player_target, 0, 7, DamageType::Thorns) == 7,
            "Double Damage doubles only normal damage");
    at_end_of_round(g, player_target);
    resolve_actions(g);
    require(power_amount(g, g.player.creature, PowerId::DoubleDamage) == 2, "monster-sourced Double Damage skips its first round");
    at_end_of_round(g, player_target);
    resolve_actions(g);
    require(power_amount(g, g.player.creature, PowerId::DoubleDamage) == 1, "Double Damage reduces on the next round");
    apply_power(g, 0, 0, PowerId::BeatOfDeath, 3);
    apply_power(g, 0, player_target, PowerId::Constricted, 2);
    apply_power(g, player_target, player_target, PowerId::Vulnerable, 1);
    const int constricted = find_power(g, g.player.creature, PowerId::Constricted);
    require(g.player.creature.powers.at(g.power_objects, constricted).state.Constricted.source == 0, "Constricted retains its constructor source");
    Effect use{};
    power_onAfterUseCard(g, g.monsters[0].creature.powers.at(g.power_objects, find_power(g, g.monsters[0].creature, PowerId::BeatOfDeath)), 0, g.hand[0], use);
    power_atEndOfTurn(g, g.player.creature.powers.at(g.power_objects, constricted), player_target, false);
    resolve_actions(g);
    require(g.player.creature.hp == hp - 5, "Beat of Death and Constricted queue raw Thorns damage without applying Vulnerable");
}

static void test_turn_hooks() {
    Game g = combat();
    apply_power(g, 0, player_target, PowerId::DrawReduction, 1);
    apply_power(g, 0, player_target, PowerId::DrawReduction, 1);
    require(g.player.hand_size == 4, "stacking Draw Reduction changes hand size only on initial application");
    step(g, {ActionKind::EndTurn});
    require(g.hand.size == 4 && power_amount(g, g.player.creature, PowerId::DrawReduction) == 2,
            "Draw Reduction skips its first end-of-round reduction");
    step(g, {ActionKind::EndTurn});
    require(g.hand.size == 4 && power_amount(g, g.player.creature, PowerId::DrawReduction) == 1,
            "queued reduction happens after the next-turn draw count is captured");
    step(g, {ActionKind::EndTurn});
    require(g.hand.size == 4 && g.player.hand_size == 5 && !has_power(g, g.player.creature, PowerId::DrawReduction),
            "Draw Reduction removal restores the future hand size");
    apply_power(g, player_target, player_target, PowerId::DrawCardNextTurn, 2);
    apply_power(g, player_target, player_target, PowerId::Energized, 2);
    step(g, {ActionKind::EndTurn});
    require(g.hand.size == 7 && g.player.energy == 5 &&
            !has_power(g, g.player.creature, PowerId::DrawCardNextTurn) && !has_power(g, g.player.creature, PowerId::Energized),
            "next-turn draw and energy resolve once and remove their powers");
    apply_power(g, player_target, player_target, PowerId::Blur, 1);
    const int attack = monster_output_damage(g, g.monsters[0], 0);
    g.player.creature.block = 100;
    step(g, {ActionKind::EndTurn});
    require(g.player.creature.block == 100 - attack && !has_power(g, g.player.creature, PowerId::Blur),
            "Blur preserves block before the queued duration removal");
    apply_power(g, player_target, player_target, PowerId::Barricade, -1);
    g.player.creature.block = 100;
    step(g, {ActionKind::EndTurn});
    require(g.player.creature.block > 0, "Barricade retains player block");
    apply_power(g, 0, 0, PowerId::Barricade, -1);
    g.monsters[0].creature.block = 20;
    Effect start{};
    start.kind = EffectKind::MonsterStart;
    execute_effect(g, start);
    require(g.monsters[0].creature.block == 20, "Barricade retains monster block before start-of-turn powers");
}

static void test_card_duplication() {
    Game g = combat();
    apply_power(g, player_target, player_target, PowerId::AfterImage, 1);
    apply_power(g, 0, 0, PowerId::BeatOfDeath, 2);
    apply_power(g, player_target, player_target, PowerId::Burst, 1);
    apply_power(g, player_target, player_target, PowerId::Duplication, 1);
    const int source = g.hand[0];
    const int hp = g.player.creature.hp;
    const int originals = g.cards.size;
    step(g, {ActionKind::Play, 0, -1});
    require(g.player.energy == 2 && g.player.cards_played_this_turn == 3 && g.player.creature.block == 12 &&
            g.player.creature.hp == hp, "two copies trigger powers, consume no energy and never copy themselves");
    require(g.discard.size == 1 && g.discard[0] == source && g.exhaust.empty(),
            "purge-on-use copies do not enter the discard or exhaust piles");
    require(g.cards.size == originals + 2 && g.cards[originals].uuid == g.cards[source].uuid &&
            g.cards[originals + 1].uuid == g.cards[source].uuid,
            "copies have distinct card instances and the source UUID");
    require(g.manager.played_cards.values[1] == originals + 1 && g.manager.played_cards.values[2] == originals,
            "front-of-queue copies execute in reverse insertion order");
    require(!has_power(g, g.player.creature, PowerId::Burst) && !has_power(g, g.player.creature, PowerId::Duplication),
            "Burst removes at the top, Duplication at the bottom");
}

int main() {
    test_constructors_and_stacking();
    test_confusion_rng();
    test_damage_hooks();
    test_turn_hooks();
    test_card_duplication();
    std::puts("Java-derived power cases passed (not oracle trace acceptance)");
}
