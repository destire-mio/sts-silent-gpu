#include "core/engine.hpp"
#include <cstdio>
#include <cstdlib>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Searing Blow mismatch: %s\n", message);
        std::exit(1);
    }
}

int main() {
    Game g{};
    g.cards.add(make_card(CardId::SearingBlow, 2));
    require(g.cards[0].base_damage == 21 && card_canUpgrade(g, 0), "repeated upgrades stay eligible");
    g.cards.add(card_makeCopy(g, 0));
    require(g.cards[1].upgrades == 2 && g.cards[1].base_damage == 12 &&
            g.cards[1].state.SearingBlow.flags == 0, "constructor copies preserve only the upgrade count");
    Card equivalent = card_makeStatEquivalentCopy(g, 1);
    require(equivalent.upgrades == 2 && equivalent.base_damage == 12 &&
            equivalent.state.SearingBlow.flags == 2,
            "stat-equivalent copies restore upgraded but keep upgradeDamage's flag");
    equivalent = card_makeStatEquivalentCopy(g, 0);
    require(equivalent.upgrades == 2 && equivalent.base_damage == 21 &&
            equivalent.state.SearingBlow.flags == 3, "upgraded stat-equivalent copies preserve damage and flags");
    card_upgrade(g.cards[1]);
    require(g.cards[1].upgrades == 3 && g.cards[1].base_damage == 18,
            "the next upgrade uses the constructor-preserved count");
}
