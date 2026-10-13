#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Card batch 06 mismatch: %s\n", message);
        std::exit(1);
    }
}

int main() {
    const CardId cards[] = {CardId::Terror, CardId::ToolsOfTheTrade};
    const int turn_costs[] = {-1, 0, 1, 3};
    const int upgraded_costs[] = {0, 0, 0, 2};
    for (CardId id : cards) {
        for (int i = 0; i < 4; ++i) {
            Card card = make_card(id);
            card.cost_for_turn = turn_costs[i];
            card.free_to_play_once = true;
            card_upgrade(card);
            require(card.cost == 0 && card.cost_for_turn == upgraded_costs[i],
                    "cost upgrade preserves a positive turn-cost delta and clamps negative costs");
            require(card.upgrades == 1 && card.free_to_play_once,
                    "upgrade changes the card instance without consuming its free play");
            card_upgrade(card);
            require(card.upgrades == 1 && card.cost_for_turn == upgraded_costs[i],
                    "a second upgrade leaves the cost unchanged");
        }
    }
}
