#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <type_traits>

using namespace spire;

static_assert(std::is_trivially_copyable_v<Game>);
static_assert(std::is_standard_layout_v<Game>);

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Combat mismatch: %s\n", message);
        std::exit(1);
    }
}

static void require_pile(const Pile& actual, std::initializer_list<int> expected, const char* message) {
    require(actual.size == static_cast<int>(expected.size()), message);
    int index = 0;
    for (int card : expected) require(actual[index++] == card, message);
}

static int hand_index(const Game& g, CardId card) {
    for (int i = 0; i < g.hand.size; ++i) if (g.cards[g.hand[i]].id == card) return i;
    require(false, "requested card is in hand");
    return -1;
}

static void play(Game& g, CardId card, int target = -1) {
    step(g, {ActionKind::Play, hand_index(g, card), target});
}

static CombatSetup default_setup(const Game& g) {
    CombatSetup setup{};
    setup.deck = g.run.master_deck;
    setup.relics = g.run.relics;
    setup.hp = g.player.creature.hp;
    setup.max_hp = g.player.creature.max_hp;
    return setup;
}

static CombatSetup deck_setup(std::initializer_list<CardId> cards) {
    CombatSetup setup{};
    for (CardId card : cards) setup.deck.push_back(make_card(card));
    return setup;
}

static void test_default_cultist() {
    Game g{};
    reset(g, 0);
    require(g.player.creature.hp == 59 && g.player.creature.max_hp == 66, "A20 starting HP");
    require(g.run.master_deck.size == 13, "A20 starter deck size");
    require(g.run.master_deck[0].id == CardId::AscendersBane && g.run.master_deck[11].id == CardId::Survivor &&
            g.run.master_deck[12].id == CardId::Neutralize, "AbstractDungeon.dungeonTransitionSetup adds AscendersBane to the top");
    CombatSetup setup = default_setup(g);
    setup.deck.push_back(setup.deck.erase(0));
    start_combat(g, setup);
    // CardGroup.initializeDeck/draw use the end of the list. Orders use target-JAR RNG and Collections.shuffle.
    require_pile(g.hand, {4,11,0,1,9,3,5}, "first hand");
    require_pile(g.draw, {10,12,2,7,8,6}, "first draw pile");
    require(card_baseBlock(g.cards[10]) == 8 && g.cards[10].block == -1 &&
            g.cards[2].base_damage == 6 && g.cards[2].damage == -1,
            "undrawn cards retain AbstractCard constructor values until applyPowers");
    require_pile(g.drawn_cards, {3,5}, "Ring of the Snake draw history");
    require(g.player.energy == 3 && g.turn == 1, "first turn energy");
    require(g.monsters[0].creature.hp == 56 && g.monsters[0].intent == Intent::Buff, "Cultist HP and first intent");
    require(g.rng.monster_hp.counter == 1 && g.rng.ai.counter == 1 && g.rng.shuffle.counter == 1, "initial RNG counters");
    ActionSet actions{};
    legal_actions(g, actions);
    require(actions.size == 8 && actions[7].kind == ActionKind::EndTurn, "initial legal actions");
    for (int i = 0; i < 7; ++i) require(actions[i].kind == ActionKind::Play && actions[i].index == i, "legal hand order");

    step(g, {ActionKind::EndTurn});
    require_pile(g.hand, {6,8,7,2,12}, "second hand");
    require_pile(g.draw, {10}, "second draw pile");
    require_pile(g.discard, {5,3,9,1,0,11,4}, "end turn discards from hand top");
    require(g.turn == 2 && g.player.creature.hp == 59 && g.player.energy == 3, "Cultist ritual turn");
    require(power_amount(g, g.monsters[0].creature, PowerId::Ritual) == 5 &&
            !has_power(g, g.monsters[0].creature, PowerId::Strength), "Ritual skips first round");
    require(g.monsters[0].intent_damage == 6, "Cultist next attack before ritual gain");

    step(g, {ActionKind::EndTurn});
    require_pile(g.hand, {10,4,8,11,6}, "third hand after refill");
    require_pile(g.draw, {0,1,7,3,9,5,2}, "refilled draw pile");
    require_pile(g.exhaust, {12}, "Ascenders Bane ethereal exhaust");
    require(g.discard.empty(), "refill clears discard pile");
    require(g.turn == 3 && g.player.creature.hp == 53, "Cultist first attack damage");
    require(power_amount(g, g.monsters[0].creature, PowerId::Strength) == 5 && g.monsters[0].intent_damage == 11, "Ritual strengthens next attack");
    require(g.rng.ai.counter == 3 && g.rng.shuffle.counter == 2, "turn and refill RNG counters");
    require(g.manager.damage_received_this_combat == 6 && g.manager.damage_received_this_turn == 0, "damage history turn reset");
}

static void test_survivor_and_slimed() {
    Game g{};
    reset(g, 0);
    start_combat(g, deck_setup({CardId::Survivor,CardId::Strike_G,CardId::Defend_G,CardId::Slimed,CardId::Dazed}));
    play(g, CardId::Survivor);
    require(g.screen == Screen::Discard && g.hand.size == 4, "Survivor asks for a discard");
    require(g.player.creature.block == 8 && g.player.energy == 2, "Survivor gains block and spends energy before selection");
    require(g.player.card_in_use == 0 && g.discard.empty(), "Survivor waits outside piles until selection");
    ActionSet actions{};
    legal_actions(g, actions);
    require(actions.size == 4, "discard choice count");
    for (int i = 0; i < 4; ++i) require(actions[i].kind == ActionKind::Choose && actions[i].index == i, "discard action order");
    step(g, {ActionKind::Choose, hand_index(g, CardId::Dazed), -1});
    require(g.screen == Screen::Discard && g.selected_cards.size == 1 && g.discard.empty(),
            "discard selection waits for confirmation");
    legal_actions(g, actions);
    require(actions.size == 1 && actions[0].kind == ActionKind::Proceed, "selected discard offers confirmation");
    step(g, {ActionKind::Proceed});
    require(g.screen == Screen::Combat && g.hand.size == 3 && g.player.card_in_use == -1, "Survivor resumes card flow");
    require_pile(g.discard, {4,0}, "chosen card precedes Survivor in discard");
    require(g.manager.total_discarded_this_turn == 1 && g.player.cards_played_this_turn == 1, "manual discard and play counts");
    play(g, CardId::Slimed);
    require_pile(g.exhaust, {3}, "Slimed exhausts on use");
    require(g.player.energy == 1 && g.player.cards_played_this_turn == 2, "Slimed cost and play count");
}

static void test_neutralize_duration() {
    Game g{};
    reset(g, 0);
    start_combat(g, deck_setup({CardId::Neutralize,CardId::Neutralize,CardId::Neutralize,CardId::Neutralize,CardId::Neutralize}));
    step(g, {ActionKind::EndTurn});
    play(g, CardId::Neutralize, 0);
    require(g.monsters[0].creature.hp == 53 && power_amount(g, g.monsters[0].creature, PowerId::Weak) == 1, "Neutralize damage then Weak");
    require(g.monsters[0].intent_damage == 4 && g.player.energy == 3, "Weak floors damage and Neutralize costs zero");
    step(g, {ActionKind::EndTurn});
    require(g.player.creature.hp == 66, "Weak applies to current monster attack");
    require(!has_power(g, g.monsters[0].creature, PowerId::Weak), "player-applied Weak expires after round");
    require(g.monsters[0].intent_damage == 11, "next intent reflects expired Weak and Ritual");
}

static void test_power_order_and_damage() {
    Game g{};
    reset(g, 0);
    start_combat(g, deck_setup({CardId::Strike_G,CardId::Defend_G,CardId::Strike_G,CardId::Defend_G,CardId::Neutralize}));
    apply_power(g, player_target, player_target, PowerId::Weak, 1);
    apply_power(g, player_target, player_target, PowerId::Frail, 1);
    apply_power(g, player_target, player_target, PowerId::Strength, 3);
    apply_power(g, player_target, player_target, PowerId::Dexterity, 4);
    apply_power(g, player_target, 0, PowerId::Vulnerable, 1);
    const Powers& powers = g.player.creature.powers;
    require(powers.size == 4 && powers.at(g.power_objects, 0).id == PowerId::Strength && powers.at(g.power_objects, 1).id == PowerId::Dexterity &&
            powers.at(g.power_objects, 2).id == PowerId::Frail && powers.at(g.power_objects, 3).id == PowerId::Weak, "stable power priority order");
    play(g, CardId::Strike_G, 0);
    require(g.monsters[0].creature.hp == 46, "damage floors after Strength Weak Vulnerable: floor(9*.75*1.5)");
    play(g, CardId::Defend_G);
    require(g.player.creature.block == 6, "block applies Dexterity before Frail: floor(9*.75)");
    apply_power(g, player_target, player_target, PowerId::Strength, 2000);
    require(power_amount(g, g.player.creature, PowerId::Strength) == 999, "Strength upper cap");
    apply_power(g, player_target, player_target, PowerId::Dexterity, -2000);
    require(power_amount(g, g.player.creature, PowerId::Dexterity) == -999, "Dexterity lower cap");
}

static void test_retained_ethereal() {
    Game g{};
    reset(g, 0);
    start_combat(g, deck_setup({CardId::Dazed,CardId::Strike_G,CardId::Defend_G,CardId::Neutralize,CardId::Slimed}));
    g.cards[0].retain = true;
    step(g, {ActionKind::EndTurn});
    require(g.exhaust.empty() && g.hand[0] == 0 && !g.cards[0].retain, "retained ethereal bypasses exhaust and clears temporary retain");
    step(g, {ActionKind::EndTurn});
    require_pile(g.exhaust, {0}, "ethereal exhausts when temporary retain expires");
}

static void test_small_deck_draw() {
    Game g{};
    reset(g, 0);
    start_combat(g, deck_setup({CardId::Defend_G,CardId::Strike_G}));
    require(g.hand.size == 2 && g.draw.empty() && g.discard.empty(), "small deck draws available cards");
    // DrawCardAction.update queues EmptyDeckShuffleAction when draw amount exceeds the deck, even with no discard.
    require(g.rng.shuffle.counter == 2, "overdraw consumes the empty shuffle seed");
}

static void test_innate_draw_order() {
    Game g{};
    reset(g, 0);
    auto setup = default_setup(g);
    for (int i = 0; i < 8; ++i) setup.deck[i].innate = true;
    start_combat(g, setup);
    // CardGroup.initializeDeck pre-turn extras drain before AbstractRoom queues Draw5 and SnakeRing Draw2.
    require_pile(g.hand, {4,0,1,3,5,6,7,2,11,9}, "innate extras precede opening and relic draws");
    require_pile(g.draw, {10,12,8}, "innate draw leaves original non-innate order");
    require_pile(g.drawn_cards, {11,9}, "Ring draw history follows innate pre-turn draw history");
    require(g.rng.shuffle.counter == 1 && g.player.energy == 3, "innate draw preserves shuffle count and opening energy");
}

static void test_null_target_skill_damage() {
    Game g{};
    reset(g, 0);
    start_combat(g, deck_setup({CardId::Defend_G,CardId::Strike_G,CardId::Strike_G,CardId::Strike_G,CardId::Strike_G}));
    apply_power(g, player_target, player_target, PowerId::Strength, 3);
    apply_power(g, player_target, 0, PowerId::Vulnerable, 1);
    require(g.cards[0].damage == 2, "untargeted hand display applies outgoing Strength");
    play(g, CardId::Defend_G);
    require_pile(g.discard, {0}, "Defend enters discard");
    // AbstractCard.calculateCardDamage(null) applies the first monster's powers even to skill damage values.
    require(g.cards[0].damage == g.cards[0].base_damage, "discard resets damage to its base value");
    require(g.player.creature.block == 5 && g.monsters[0].creature.hp == 56, "skill damage field does not cause an attack");
}

static void test_damage_hooks() {
    Game g{};
    reset(g, 0);
    auto setup = deck_setup({CardId::Strike_G,CardId::Strike_G,CardId::Strike_G,CardId::Strike_G,CardId::Strike_G});
    setup.encounter = EncounterId::TwoLouse;
    start_combat(g, setup);
    const int hp = g.monsters[0].creature.hp;
    const int other_hp = g.monsters[1].creature.hp;
    const int curl = power_amount(g, g.monsters[0].creature, PowerId::CurlUp);
    require(curl >= 9 && curl <= 12, "A20 louse Curl Up amount");
    g.monsters[0].creature.block = 6;
    play(g, CardId::Strike_G, 0);
    require(g.monsters[0].creature.hp == hp && has_power(g, g.monsters[0].creature, PowerId::CurlUp), "blocked attack does not trigger Curl Up");
    play(g, CardId::Strike_G, 0);
    require(g.monsters[0].creature.hp == hp - 6 && g.monsters[0].creature.block == curl &&
            !has_power(g, g.monsters[0].creature, PowerId::CurlUp), "Curl Up gains block after nonlethal HP damage");
    play(g, CardId::Strike_G, 0);
    require(g.monsters[0].creature.hp == hp - 6 && g.monsters[0].creature.block == curl - 6, "Curl Up block protects later attacks");
    require(g.monsters[1].creature.hp == other_hp && has_power(g, g.monsters[1].creature, PowerId::CurlUp), "targeted hits leave second louse unchanged");

    reset(g, 0);
    setup.encounter = EncounterId::TwoFungiBeasts;
    start_combat(g, setup);
    g.monsters[0].creature.hp = 6;
    g.monsters[1].creature.hp = 6;
    play(g, CardId::Strike_G, 0);
    require(g.screen == Screen::Combat && power_amount(g, g.player.creature, PowerId::Vulnerable) == 2, "first Fungi death applies Spore Cloud");
    require(!g.player.creature.powers.at(g.power_objects, 0).just_applied, "player-turn Spore Cloud does not skip round decrement");
    require(g.monsters[1].creature.hp == 6, "first Fungi death leaves second Fungi alive");
    play(g, CardId::Strike_G, 1);
    require(g.screen == Screen::Victory && power_amount(g, g.player.creature, PowerId::Vulnerable) == 2, "last Fungi death suppresses Spore Cloud");
}

static void test_artifact() {
    Game g{};
    reset(g, 0);
    start_combat(g, deck_setup({CardId::Neutralize,CardId::Defend_G,CardId::Strike_G,CardId::Defend_G,CardId::Strike_G}));
    apply_power(g, 0, 0, PowerId::Artifact, 1);
    play(g, CardId::Neutralize, 0);
    require(g.monsters[0].creature.hp == 53 && !has_power(g, g.monsters[0].creature, PowerId::Weak) &&
            !has_power(g, g.monsters[0].creature, PowerId::Artifact), "Artifact blocks Weak and consumes one charge after damage");
    apply_power(g, player_target, player_target, PowerId::Artifact, 1);
    apply_power(g, player_target, player_target, PowerId::Strength, 0);
    play(g, CardId::Defend_G);
    require(!has_power(g, g.player.creature, PowerId::Artifact) && !has_power(g, g.player.creature, PowerId::Strength), "zero Strength application has debuff type");
}

static void test_terminal_states() {
    Game g{};
    reset(g, 0);
    start_combat(g, deck_setup({CardId::Neutralize,CardId::Defend_G,CardId::Strike_G,CardId::Defend_G,CardId::Strike_G}));
    g.monsters[0].creature.hp = 3;
    play(g, CardId::Neutralize, 0);
    require(g.screen == Screen::Victory && g.battle_over, "lethal attack reaches victory");
    require(g.monsters[0].creature.hp == 0 && !has_power(g, g.monsters[0].creature, PowerId::Weak), "lethal Neutralize cancels pending Weak");
    require_pile(g.discard, {0}, "victory preserves UseCardAction");
    ActionSet actions{};
    legal_actions(g, actions);
    require(actions.empty(), "combat victory has no combat decisions");

    reset(g, 0);
    auto setup = deck_setup({CardId::Defend_G,CardId::Defend_G,CardId::Defend_G,CardId::Defend_G,CardId::Defend_G});
    setup.hp = 1;
    start_combat(g, setup);
    step(g, {ActionKind::EndTurn});
    step(g, {ActionKind::EndTurn});
    require(g.screen == Screen::Death && g.player.creature.hp == 0 && g.player.creature.dead, "lethal monster attack reaches death");
    legal_actions(g, actions);
    require(actions.empty(), "death has no decisions");
}

static void test_played_card_history() {
    Game g{};
    reset(g, 0);
    auto setup = deck_setup({CardId::Strike_G,CardId::Defend_G});
    start_combat(g, setup);
    play(g, CardId::Strike_G, 0);
    play(g, CardId::Defend_G);
    const PlayedCards& history = g.manager.played_cards;
    require(history.size == 2 && history.turn_start == 0 && history.values[0] == 0 && history.values[1] == 1,
            "played history preserves first-turn card identities and order");
    step(g, {ActionKind::EndTurn});
    require(history.size == 2 && history.turn_start == 2,
            "new turn clears turn history without losing combat history");
    play(g, CardId::Strike_G, 0);
    require(history.size == 3 && history.turn_start == 2 && history.values[2] == history.values[0],
            "replayed card retains the same identity in both histories");
    step(g, {ActionKind::EndTurn});
    g.monsters[0].creature.hp = 6;
    play(g, CardId::Strike_G, 0);
    require(g.screen == Screen::Victory && history.size == 0 && history.turn_start == 0,
            "victory clears both played histories");
    start_combat(g, setup);
    require(history.size == 0 && history.turn_start == 0, "new combat begins with empty played histories");
}

int main() {
    test_default_cultist();
    test_survivor_and_slimed();
    test_neutralize_duration();
    test_power_order_and_damage();
    test_retained_ethereal();
    test_small_deck_draw();
    test_innate_draw_order();
    test_null_target_skill_damage();
    test_damage_hooks();
    test_artifact();
    test_terminal_states();
    test_played_card_history();
    std::puts("Java-derived combat cases passed (not oracle trace acceptance)");
}
