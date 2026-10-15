#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

static Game combat(PotionId first, PotionId second = PotionId::Empty) {
    Game g{};
    reset(g, 102);
    CombatSetup setup{};
    setup.hp = 40;
    setup.max_hp = 66;
    setup.deck.push_back(make_card(CardId::Survivor));
    for (int i = 0; i < 4; ++i) setup.deck.push_back(make_card(CardId::Strike_G));
    setup.potions[0] = first;
    setup.potions[1] = second;
    start_combat(g, setup);
    return g;
}

static void potion_during_selection() {
    Game g = combat(PotionId::FruitJuice, PotionId::StrengthPotion);
    int survivor = 0;
    while (g.cards[g.hand[survivor]].id != CardId::Survivor) ++survivor;
    step(g, {ActionKind::Play, survivor, -1});
    require(g.screen == Screen::Discard, "Survivor opens hand selection");
    const int current = g.manager.current_action;
    const int selected = g.hand[0];
    ActionSet legal{};
    legal_actions(g, legal);
    require(legal[0].kind == ActionKind::PotionUse && legal[0].index == 0 &&
            legal[2].kind == ActionKind::PotionUse && legal[2].index == 1,
            "occupied potion slots precede hand choices");
    step(g, {ActionKind::PotionUse, 0, -1});
    require(g.player.creature.hp == 45 && g.player.creature.max_hp == 71 &&
            g.screen == Screen::Discard && g.manager.current_action == current,
            "Fruit Juice heals without closing the pending selection");
    step(g, {ActionKind::PotionUse, 1, -1});
    require(!has_power(g, g.player.creature, PowerId::Strength) && g.screen == Screen::Discard,
            "a queued potion power waits behind the pending selection");
    step(g, {ActionKind::Choose, 0, -1});
    step(g, {ActionKind::Proceed, -1, -1});
    require(g.discard.size == 2 && g.discard[0] == selected &&
            power_amount(g, g.player.creature, PowerId::Strength) == 2,
            "discard and potion power settle after confirmation");
    Game control = combat(PotionId::StrengthPotion);
    require(control.player.creature.hp == 40 &&
            !has_power(control, control.player.creature, PowerId::Strength),
            "another game retains its own health and powers");
}

static void potion_availability() {
    Game g = combat(PotionId::SmokeBomb);
    require(potion_canUse(g, 0), "Smoke Bomb is usable against a normal enemy");
    apply_power(g, player_target, 0, PowerId::BackAttack, 1);
    require(!potion_canUse(g, 0), "Back Attack prevents Smoke Bomb use");
    g.monsters[0].creature.dying = true;
    require(!potion_canUse(g, 0), "Smoke Bomb inspects dying enemies for Back Attack");
    g = combat(PotionId::FruitJuice);
    g.event.id = EventId::WeMeetAgain;
    require(!potion_canUse(g, 0) && !potion_canDiscard(g, 0),
            "We Meet Again prevents Fruit Juice use and discard");
    g.event.id = EventId::Count;
    g.screen = Screen::None;
    g.room_in_combat = false;
    g.manager.turn_has_ended = true;
    require(potion_canUse(g, 0), "an ended combat turn does not block Fruit Juice outside combat");
    g = combat(PotionId::GamblersBrew);
    g.hand.clear();
    step(g, {ActionKind::PotionUse, 0, -1});
    require(g.screen == Screen::Combat && g.manager.total_discarded_this_turn == 0,
            "Gambler's Brew consumes without opening an empty-hand selection");
}

static void smoke_during_setup() {
    Game g{};
    reset(g, 202610212024);
    CombatSetup setup{};
    setup.deck.push_back(make_card(CardId::Setup));
    setup.deck.push_back(make_card(CardId::Blur));
    for (int i = 0; i < 3; ++i) setup.deck.push_back(make_card(CardId::Defend_G));
    setup.potions[0] = PotionId::SmokeBomb;
    start_combat(g, setup);
    int index = 0;
    while (g.cards[g.hand[index]].id != CardId::Setup) ++index;
    const int played = g.hand[index];
    step(g, {ActionKind::Play, index, -1});
    index = 0;
    while (g.cards[g.hand[index]].id != CardId::Blur) ++index;
    const int selected = g.hand[index];
    step(g, {ActionKind::Choose, index, -1});
    step(g, {ActionKind::PotionUse, 0, -1});
    require(g.screen == Screen::Discard && g.player.escaping, "Smoke Bomb leaves Setup confirmation available");
    step(g, {ActionKind::Proceed, -1, -1});
    require(g.screen == Screen::CombatReward && g.draw.size == 1 && g.draw[0] == selected &&
            g.cards[selected].free_to_play_once && g.discard.size == 1 && g.discard[0] == played &&
            g.player.card_in_use == -1,
            "Setup retrieves its free card and UseCard discards Setup before escape ends combat");
}

static void smoke_during_retain() {
    Game g{};
    reset(g, 202610212024);
    CombatSetup setup{};
    setup.deck.push_back(make_card(CardId::Dazed));
    for (int i = 0; i < 4; ++i) setup.deck.push_back(make_card(CardId::Defend_G));
    setup.potions[0] = PotionId::SmokeBomb;
    start_combat(g, setup);
    apply_power(g, player_target, player_target, PowerId::RetainCard, 1);
    const auto drawn = g.drawn_cards;
    step(g, {ActionKind::EndTurn, -1, -1});
    require(g.screen == Screen::Discard, "Well-Laid Plans opens the end-turn selection");
    step(g, {ActionKind::PotionUse, 0, -1});
    int index = 0;
    while (g.cards[g.hand[index]].id == CardId::Dazed) ++index;
    const int retained = g.hand[index];
    step(g, {ActionKind::Choose, index, -1});
    step(g, {ActionKind::Proceed, -1, -1});
    require(g.screen == Screen::CombatReward && g.hand.size == 1 && g.hand[0] == retained &&
            g.discard.size == 3 && g.exhaust.size == 1 && g.drawn_cards.size == drawn.size,
            "escape allows end-turn discard and retained-card restoration but interrupts the next turn");
    for (int i = 0; i < drawn.size; ++i)
        require(g.drawn_cards[i] == drawn[i], "escape preserves the previous draw history");
}

int main() {
    potion_during_selection();
    potion_availability();
    smoke_during_setup();
    smoke_during_retain();
}
