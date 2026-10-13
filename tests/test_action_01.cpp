#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;
using namespace spire::units::actions;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

static Game combat(int count = 5) {
    Game g{};
    reset(g, 101);
    CombatSetup setup{};
    setup.hp = 59;
    setup.max_hp = 66;
    for (int i = 0; i < count; ++i) setup.deck.push_back(make_card(CardId::Strike_G));
    start_combat(g, setup);
    return g;
}

static void discard_paths() {
    Game g = combat();
    Pile expected = g.hand;
    Random random = g.rng.card_random;
    const int first = expected.erase(random.random(expected.size - 1));
    const int second = expected.erase(random.random(expected.size - 1));
    Effect e{};
    e.kind = EffectKind::Discard;
    e.amount = 2;
    e.extra = 1;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.discard.size == 2 && g.discard[0] == first && g.discard[1] == second,
            "random discard removes each sampled card before the next sample");
    require(g.rng.card_random.counter == random.counter && g.manager.total_discarded_this_turn == 2,
            "random discard uses cardRandomRng once per card");
    e = {};
    e.kind = EffectKind::Discard;
    e.amount = -1;
    add_bottom(g, e);
    run_until_decision(g);
    ActionSet legal{};
    legal_actions(g, legal);
    require(legal.size == 4 && legal[3].kind == ActionKind::Proceed,
            "negative discard amount permits zero and any number");
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Proceed});
    require(g.hand.size == 2 && g.discard.size == 3 && g.manager.total_discarded_this_turn == 3,
            "optional discard resumes its action after confirmation");
    const int card = g.hand[0];
    add_bottom(g, DiscardSpecificCard::make(card));
    run_until_decision(g);
    require(g.hand.size == 1 && g.discard[g.discard.size - 1] == card,
            "specific discard preserves card identity");
    const int total = g.manager.total_discarded_this_turn;
    add_bottom(g, DiscardSpecificCard::make(card));
    run_until_decision(g);
    require(g.manager.total_discarded_this_turn == total, "absent specific card has no discard callbacks");
}

static void exhaust_paths() {
    Game g = combat();
    const int first = g.hand[g.hand.size - 1];
    const int last = g.hand[0];
    add_bottom(g, Exhaust::make(99, false));
    run_until_decision(g);
    require(g.hand.empty() && g.exhaust.size == 5 && g.exhaust[0] == first && g.exhaust[4] == last,
            "exhaust-all walks the hand from top to bottom");
    require(g.exhaust_action_num == 5, "exhaust-all publishes actual hand size");
    g = combat();
    Random random = g.rng.card_random;
    Pile expected = g.hand;
    const int chosen = expected.erase(random.random(expected.size - 1));
    add_bottom(g, Exhaust::make(1, true, false, false));
    run_until_decision(g);
    require(g.exhaust.size == 1 && g.exhaust[0] == chosen && g.rng.card_random.counter == random.counter,
            "random exhaust consumes cardRandomRng");
    add_bottom(g, Exhaust::make(3, false, true, true));
    run_until_decision(g);
    const int selected = g.hand[0];
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Proceed});
    require(g.exhaust.size == 2 && g.exhaust[1] == selected && g.hand.size == 3,
            "any-number exhaust preserves selected card identity");
    const int card = g.hand.erase(0);
    g.discard.push_back(card);
    g.cards[card].free_to_play_once = true;
    g.cards[card].exhaust_on_use_once = true;
    add_bottom(g, ExhaustCard::make(card, PileId::Discard));
    run_until_decision(g);
    require(g.discard.empty() && g.exhaust[g.exhaust.size - 1] == card &&
            !g.cards[card].free_to_play_once && !g.cards[card].exhaust_on_use_once,
            "specific exhaust removes from its supplied group and clears one-use flags");
    add_bottom(g, ExhaustCard::make(card, PileId::Discard));
    run_until_decision(g);
    require(g.exhaust.size == 3, "specific exhaust ignores a card absent from its supplied group");
}

static void discard_to_hand() {
    Game g = combat(2);
    const int first = g.hand.erase(0);
    const int second = g.hand.erase(0);
    g.discard.push_back(first);
    g.discard.push_back(second);
    add_bottom(g, BetterDiscardPileToHand::make(2, 0));
    run_until_decision(g);
    require(g.hand.size == 2 && g.hand[0] == first && g.hand[1] == second && g.discard.empty() &&
            g.cards[first].cost_for_turn == 0, "discard-to-hand retains pile order and changes turn cost");
    discard_card(g, 0, false);
    discard_card(g, 0, false);
    add_bottom(g, BetterDiscardPileToHand::make(1));
    run_until_decision(g);
    require(g.screen == Screen::Grid && g.discard.size == 2, "discard-to-hand opens a grid without moving cards");
    step(g, {ActionKind::Choose, 1});
    require(g.screen == Screen::Combat && g.hand.size == 1 && g.hand[0] == second && g.discard[0] == first,
            "fixed grid selection closes at its requested count");
    discard_card(g, 0, false);
    add_bottom(g, BetterDiscardPileToHand::make(2, true));
    run_until_decision(g);
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Choose, 0});
    step(g, {ActionKind::Proceed});
    require(g.hand.empty() && g.discard.size == 2, "optional grid can toggle its choice and confirm zero");
    add_bottom(g, BetterDiscardPileToHand::make(1, true));
    run_until_decision(g);
    step(g, {ActionKind::Choose, 0});
    require(g.screen == Screen::Combat && g.hand[0] == first,
            "optional grid closes at its limit when cards remain unselected");
    discard_card(g, 0, false);
    for (int i = 0; i < 9; ++i) {
        const int card = g.cards.size;
        g.cards.push_back(make_card(CardId::Defend_G));
        g.hand.push_back(card);
    }
    add_bottom(g, BetterDiscardPileToHand::make(2));
    run_until_decision(g);
    require(g.hand.size == 10 && g.discard.size == 1 && g.discard[0] == first,
            "automatic return leaves overflow cards in the discard pile");
}

static void damage_and_power() {
    Game g = combat();
    g.player.creature.dying = true;
    Effect e{};
    e.kind = EffectKind::LoseHP;
    e.source = player_target;
    e.target = 0;
    e.amount = 3;
    const int hp = g.monsters[0].creature.hp;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.monsters[0].creature.hp == hp - 3, "LoseHP does not cancel when its source is dying");
    g = combat();
    g.monsters[0].creature.block = 20;
    e = {};
    e.kind = EffectKind::LoseHP;
    e.target = 0;
    e.amount = 3;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.monsters[0].creature.block == 20, "HP loss bypasses block");
    g = combat();
    queue_power(g, player_target, player_target, PowerId::Strength, 2);
    run_until_decision(g);
    const int before = g.monsters[0].creature.hp;
    add_bottom(g, DamageAllEnemies::make(3, DamageType::Thorns));
    run_until_decision(g);
    require(g.monsters[0].creature.hp == before - 5,
            "base-damage matrix calculates NORMAL damage before constructing the requested damage type");
    g = combat();
    Power power{PowerId::Strength, 4};
    power_initialize_state(power);
    power_initialPower(g, power, player_target);
    add_bottom(g, ApplyPower::make(player_target, player_target, power, 2));
    run_until_decision(g);
    require(power_amount(g, g.player.creature, PowerId::Strength) == 4, "initial power uses constructed amount");
    add_bottom(g, ApplyPower::make(player_target, player_target, power, 2));
    run_until_decision(g);
    require(power_amount(g, g.player.creature, PowerId::Strength) == 6, "existing power uses raw stack amount");
}

static void temporary_cards_and_queue() {
    Game g = combat();
    const int original = g.hand[0];
    add_bottom(g, AddCardDiscard::make(original, true));
    add_bottom(g, AddCardDiscard::make(original, 5));
    add_bottom(g, AddCardDiscard::make(original, 6));
    run_until_decision(g);
    require(g.discard.size == 6 && g.cards[g.discard[0]].uuid == g.cards[original].uuid &&
            g.discard[0] != original && g.cards[g.discard[1]].uuid != g.cards[original].uuid,
            "temporary cards preserve requested UUID equality and six-card requests create nothing");
    const int limbo = g.discard[0];
    g.limbo.push_back(limbo);
    g.manager.card_queue.push_back({limbo, 0});
    g.manager.card_queue.push_back({original, 0});
    Effect e{};
    e.kind = EffectKind::ClearCardQueue;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.limbo.empty() && g.manager.card_queue.empty() && g.exhaust.empty(),
            "clearing queued limbo cards does not trigger gameplay exhaust");
    g.end_turn_enabled = false;
    e = {};
    e.kind = EffectKind::EnableEndTurnButton;
    add_bottom(g, e);
    run_until_decision(g);
    require(g.end_turn_enabled, "enable end-turn action restores the control");
    e = {};
    e.kind = EffectKind::Heal;
    e.amount = 2;
    add_bottom(g, e);
    e = {};
    e.kind = EffectKind::GainEnergy;
    add_bottom(g, e);
    e = {};
    e.kind = EffectKind::LoseHP;
    add_bottom(g, e);
    clear_post_combat_actions(g);
    require(g.manager.actions.size == 2 && g.action_objects[g.manager.actions[0]].kind == EffectKind::Heal &&
            g.action_objects[g.manager.actions[1]].kind == EffectKind::LoseHP, "post-combat cleanup preserves healing and damage actions");
}

int main() {
    discard_paths();
    exhaust_paths();
    discard_to_hand();
    damage_and_power();
    temporary_cards_and_queue();
    std::puts("action behavior checks passed");
}
