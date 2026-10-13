#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Card 04 mismatch: %s\n", message);
        std::exit(1);
    }
}

static Game combat() {
    Game g{};
    reset(g, 42);
    CombatSetup setup{};
    setup.deck.push_back(make_card(CardId::Eviscerate, 1));
    start_combat(g, setup);
    return g;
}

static void copy_source_cost() {
    Game g = combat();
    const int source = g.hand[0];
    g.manager.total_discarded_this_turn = 2;
    g.cards[source].cost_for_turn = 0;
    Card plain = card_makeCopy(g, source);
    require(plain.cost_for_turn == 3 && plain.upgrades == 0 && g.cards[source].cost_for_turn == 1,
            "Eviscerate.makeCopy changes the source cost and returns an unupgraded card");
    g.cards[source].cost_for_turn = 0;
    Card equivalent = card_makeStatEquivalentCopy(g, source);
    require(equivalent.cost_for_turn == 1 && equivalent.upgrades == 1 && equivalent.base_damage == 9 &&
            g.cards[source].cost_for_turn == 1,
            "stat-equivalent copies observe the source mutation before copying its turn cost");
    g.battle_over = true;
    g.cards[source].cost_for_turn = 0;
    card_makeCopy(g, source);
    require(g.cards[source].cost_for_turn == 0, "copies outside combat leave the source cost alone");
}

static void doppelganger_constructor() {
    Game g = combat();
    g.player.energy = 3;
    add_bottom(g, units::actions::Doppelganger::construct(false, true, 2));
    run_until_decision(g);
    require(g.player.energy == 0 && power_amount(g, g.player.creature, PowerId::Energized) == 2 &&
            power_amount(g, g.player.creature, PowerId::DrawCardNextTurn) == 2,
            "DoppelgangerAction ignores its freeToPlayOnce constructor argument and consumes energy");
}

int main() {
    copy_source_cost();
    doppelganger_constructor();
    std::puts("Card 04 Java-derived cases passed (not oracle trace acceptance)");
}
