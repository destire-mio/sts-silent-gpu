#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;
using namespace spire::units::actions;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "Action 04 mismatch: %s\n", message); std::exit(1); }
}

static Game combat() {
    Game g{};
    reset(g, 42);
    g.screen = Screen::Combat;
    g.player.energy = 3;
    build_encounter(g, EncounterId::Cultist);
    return g;
}

static int card(Game& g, CardId id, Pile& pile) {
    const int index = g.cards.size;
    g.cards.push_back(make_card(id));
    g.cards[index].uuid = index + 1;
    pile.push_back(index);
    return index;
}

static void test_setup() {
    Game g = combat();
    add_bottom(g, Setup::construct());
    run_until_decision(g);
    require(g.hand.empty() && g.draw.empty(), "empty Setup completes");
    const int first = card(g, CardId::Defend_G, g.hand);
    const int second = card(g, CardId::Neutralize, g.hand);
    g.cards[first].cost_for_turn = 2;
    add_bottom(g, Setup::construct());
    run_until_decision(g);
    require(g.screen == Screen::Discard && g.hand.size == 2, "Setup pauses for one hand selection");
    step(g, {ActionKind::Choose, 0});
    require(g.hand.size == 1 && g.selected_cards[0] == first, "selection retains card identity");
    step(g, {ActionKind::Proceed});
    require(g.draw.size == 1 && g.draw[0] == first && g.cards[first].free_to_play_once &&
            g.cards[first].cost_for_turn == 2 && g.manager.total_discarded_this_turn == 0,
            "Setup moves selected card to top without discarding or resetting its turn cost");
    add_bottom(g, Setup::construct());
    run_until_decision(g);
    require(g.draw[1] == second && !g.cards[second].free_to_play_once, "zero-cost single card is moved without making it free");
}

static void test_draw_pile_transfers() {
    Game g = combat();
    const int attack = card(g, CardId::Strike_G, g.draw);
    const int first = card(g, CardId::Defend_G, g.draw);
    const int second = card(g, CardId::Survivor, g.draw);
    add_bottom(g, SkillFromDeckToHand::construct(2));
    run_until_decision(g);
    require(g.screen == Screen::Grid && g.grid_cards.size == 2 && g.rng.card_random.counter == 1,
            "two skills require one insertion RNG call and a grid decision");
    ActionSet actions{};
    legal_actions(g, actions);
    require(actions.size == 2, "grid exposes both cards");
    step(g, {ActionKind::Choose, 0});
    require(g.screen == Screen::Grid && g.selected_cards.size == 1, "grid waits for the requested count");
    legal_actions(g, actions);
    require(actions.size == 2, "grid permits deselecting a chosen card");
    step(g, {ActionKind::Choose, 0});
    require(g.selected_cards.empty(), "selecting the same grid card toggles it off");
    step(g, {ActionKind::Choose, 1});
    step(g, {ActionKind::Choose, 0});
    require(g.hand.size == 2 && g.hand[0] == first && g.hand[1] == second &&
            g.draw.size == 1 && g.draw[0] == attack, "grid transfers skills in selection order");

    Game full = combat();
    for (int i = 0; i < 9; ++i) card(full, CardId::Strike_G, full.hand);
    card(full, CardId::Defend_G, full.draw);
    card(full, CardId::Survivor, full.draw);
    card(full, CardId::Neutralize, full.draw);
    const int initial_shuffle = full.rng.shuffle.counter;
    add_bottom(full, DrawPileToHand::construct(2, CardType::Skill));
    run_until_decision(full);
    require(full.hand.size == 10 && full.discard.size == 1 && full.draw.size == 1 &&
            full.rng.card_random.counter == 1 && full.rng.shuffle.counter == initial_shuffle + 2,
            "each transfer shuffles remaining candidates; overflow goes to discard");
    require(full.manager.total_discarded_this_turn == 0, "overflow transfer is not a manual discard");
}

static void test_draw_follow_up() {
    Game g = combat();
    const int top = card(g, CardId::Neutralize, g.draw);
    card(g, CardId::Defend_G, g.discard);
    card(g, CardId::Survivor, g.discard);
    g.player.creature.block = 5;
    const int follow = hold_action(g, LoseBlock::construct(player_target, player_target, 2));
    add_bottom(g, Draw::construct(3, true, follow));
    run_until_decision(g);
    require(g.hand.size == 3 && g.drawn_cards.size == 3 && g.drawn_cards[0] == top &&
            g.player.creature.block == 3 && g.rng.shuffle.counter == 1,
            "shuffle continuation preserves draw history and executes follow-up once");
    apply_power(g, player_target, player_target, PowerId::NoDraw, -1);
    add_bottom(g, Draw::construct(4, true, follow));
    run_until_decision(g);
    require(g.drawn_cards.empty() && g.player.creature.block == 3 && g.hand.size == 3,
            "GameActionManager skips the same completed follow-up object");
    add_bottom(g, Draw::construct(g, 4, LoseBlock::construct(player_target, player_target, 2)));
    run_until_decision(g);
    require(g.drawn_cards.empty() && g.player.creature.block == 1 && g.hand.size == 3,
            "No Draw clears history and still executes a fresh follow-up");
    add_bottom(g, LoseBlock::construct(player_target, player_target, -4));
    run_until_decision(g);
    require(g.player.creature.block == 5, "negative LoseBlock increases block as Java does");
}

static void test_card_queue() {
    Game g = combat();
    const int first = card(g, CardId::Strike_G, g.hand);
    const int second = card(g, CardId::Defend_G, g.hand);
    const int third = card(g, CardId::Neutralize, g.hand);
    g.manager.card_queue.push_back({first, 0, 3});
    g.manager.card_queue.push_back({second, no_target, 3});
    Effect queue = NewQueueCard::construct_random(third, true, true, true);
    execute_effect(g, queue);
    require(g.manager.card_queue.size == 3 && g.manager.card_queue[0].card == first &&
            g.manager.card_queue[1].card == third && g.manager.card_queue[2].card == second &&
            g.manager.card_queue[1].autoplay_card && g.manager.card_queue[1].random_target &&
            g.rng.card_random.counter == 0, "immediate insertion is second; random target selection is deferred");
    execute_effect(g, queue);
    require(g.manager.card_queue.size == 3, "duplicate queue card is ignored by object identity");
    Effect end = NewQueueCard::construct();
    execute_effect(g, end);
    execute_effect(g, end);
    require(g.manager.card_queue.size == 4 && g.manager.card_queue[3].card == -1,
            "end-turn queue marker is appended once");
    g.manager.card_queue.erase(3);
    run_until_decision(g);
    require(g.player.energy == 1 && g.discard.size == 3 && g.rng.card_random.counter == 1 &&
            !g.cards[third].in_autoplay && !g.cards[third].free_to_play_once,
            "queued autoplay chooses one target, skips payment, and clears autoplay after use");
}

static void test_damage_and_power() {
    Game g = combat();
    DamageInfo info{};
    info.source = player_target;
    info.amount = 4;
    info.info_base = 7;
    apply_power(g, player_target, 0, PowerId::Vulnerable, 2);
    const int hp = g.monsters[0].creature.hp;
    add_bottom(g, DamageRandomEnemy::construct(info));
    run_until_decision(g);
    require(g.monsters[0].creature.hp == hp - 4 && g.rng.card_random.counter == 1,
            "random hit uses precomputed damage and consumes RNG even for one enemy");
    const int attack = card(g, CardId::Strike_G, g.hand);
    g.manager.played_cards.record(attack);
    g.manager.played_cards.record(attack);
    g.manager.played_cards.record(attack);
    add_bottom(g, DamagePerAttackPlayed::construct(0, info));
    run_until_decision(g);
    require(g.monsters[0].creature.hp == hp - 12, "Finisher counts repeated attack instances and subtracts its own play");
    Power incoming(PowerId::Strength, 3);
    add_bottom(g, ApplyPowerToRandomEnemy::construct(player_target, incoming, 8));
    run_until_decision(g);
    require(power_amount(g, g.monsters[0].creature, PowerId::Strength) == 3,
            "random power preserves constructed amount separately from raw stack amount");
    add_bottom(g, ApplyPowerToRandomEnemy::construct(player_target, incoming, 8));
    run_until_decision(g);
    require(power_amount(g, g.monsters[0].creature, PowerId::Strength) == 11,
            "random power uses raw amount when stacking");
    apply_power(g, player_target, player_target, PowerId::Strength, 3);
    const Power old = g.player.creature.powers.at(g.power_objects, 0);
    const Effect reduce_old = ReducePower::construct(player_target, 0, old, 1);
    remove_power(g, player_target, PowerId::Strength);
    apply_power(g, player_target, player_target, PowerId::Strength, 10);
    add_bottom(g, reduce_old);
    run_until_decision(g);
    require(power_amount(g, g.player.creature, PowerId::Strength) == 10 &&
            power_instance(g, old.instance).amount == 2,
            "instance reduction changes removed original power without touching its replacement");
    add_bottom(g, ReducePower::construct(player_target, 0, PowerId::Strength, 10));
    run_until_decision(g);
    require(!has_power(g, g.player.creature, PowerId::Strength), "ID reduction schedules removal of the resolved instance");
}

static void test_empty_targets_and_failed_autoplay() {
    Game g = combat();
    g.monsters[0].creature.dying = true;
    DamageInfo info{};
    info.source = player_target;
    info.amount = 4;
    add_bottom(g, DamageRandomEnemy::construct(info));
    add_bottom(g, BouncingFlask::construct(no_target, 3, 3));
    add_bottom(g, DoublePoison::construct(0, player_target));
    add_bottom(g, Doppelganger::construct(false, false, 0));
    run_until_decision(g);
    require(g.rng.card_random.counter == 0 && g.player.energy == 3 && g.manager.actions.empty(),
            "empty-target branches consume no RNG; zero-effect Doppelganger preserves energy");

    Game autoplay = combat();
    const int status = card(autoplay, CardId::Dazed, autoplay.limbo);
    add_bottom(autoplay, NewQueueCard::construct(status, no_target, false, true));
    run_until_decision(autoplay);
    require(autoplay.limbo.empty() && autoplay.discard.size == 1 && autoplay.discard[0] == status &&
            autoplay.manager.played_cards.size == 0 && autoplay.player.energy == 3 &&
            !autoplay.cards[status].in_autoplay && !autoplay.cards[status].dont_trigger_on_use_card,
            "failed autoplay cleans up its card without recording or charging a play");
}

static void test_spawn_and_limbo() {
    Game g = combat();
    Monster spawned = construct_monster(g, MonsterId::Cultist);
    position_monster(spawned, -100.0f);
    g.monsters.push_back(spawned);
    add_bottom(g, SpawnMonster::construct(1, false));
    const int ai = g.rng.ai.counter;
    run_until_decision(g);
    require(g.monster_order.size == 2 && g.monster_order[0] == 1 && g.rng.ai.counter == ai + 1,
            "spawn initializes once and sorts by drawX");
    const int held = card(g, CardId::Defend_G, g.limbo);
    g.player.card_in_use = held;
    add_bottom(g, Unlimbo::construct(held, true));
    run_until_decision(g);
    require(g.limbo.empty() && g.exhaust.empty() && g.player.card_in_use == held,
            "Unlimbo's exhaust argument is visual and leaves cardInUse unchanged");
    g.limbo.push_back(held);
    add_bottom(g, ShowCardAndPoof::construct(held));
    run_until_decision(g);
    require(g.limbo.empty() && g.player.card_in_use == -1 && g.exhaust.empty(),
            "ShowCardAndPoof removes limbo and clears cardInUse without exhausting");
    g.limbo.push_back(held);
    g.player.card_in_use = held;
    add_bottom(g, ShowCard::construct(held));
    run_until_decision(g);
    require(g.limbo.empty() && g.player.card_in_use == -1, "ShowCard clears the same rule state");
    card(g, CardId::Strike_G, g.draw);
    card(g, CardId::Neutralize, g.draw);
    add_bottom(g, Expertise::construct(player_target, 2));
    run_until_decision(g);
    require(g.hand.size == 2, "Expertise draws to its requested hand size");
}

int main() {
    test_setup();
    test_draw_pile_transfers();
    test_draw_follow_up();
    test_card_queue();
    test_damage_and_power();
    test_spawn_and_limbo();
    test_empty_targets_and_failed_autoplay();
    std::puts("Action 04 Java-derived cases passed (not oracle trace acceptance)");
}
