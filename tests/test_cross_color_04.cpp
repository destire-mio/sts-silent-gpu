#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Cross-colour card batch 04 mismatch: %s\n", message);
        std::exit(1);
    }
}

int main() {
    const Card halt = make_card(CardId::Halt, 1);
    require(halt.base_magic == 14 && halt.magic == 9 && halt.block == 3,
            "Halt upgrade preserves constructor magic and block");
    const Card prostrate = make_card(CardId::Prostrate, 1);
    require(prostrate.base_magic == 3 && prostrate.magic == 3,
            "upgradeMagicNumber changes base and current magic");
    Game game{};
    game.cards.add(halt);
    game.cards[0].magic = 20;
    const Card copy = card_makeStatEquivalentCopy(game, 0);
    require(copy.base_magic == 14 && copy.magic == 9,
            "stat-equivalent copy takes current magic from its constructor and upgrade");
}
