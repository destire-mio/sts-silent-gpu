#include "core/engine.hpp"

using namespace spire;

int main() {
    Card blood = make_card(CardId::BloodForBlood);
    blood.cost_for_turn = 1;
    update_card_cost(blood, -1);
    SPIRE_ASSERT(blood.cost == 3 && blood.cost_for_turn == 0);
    card_upgrade(blood);
    SPIRE_ASSERT(blood.cost == 2 && blood.cost_for_turn == 0);
    SPIRE_ASSERT(blood.base_damage == 22 && blood.upgraded_cost);
    update_card_cost(blood, -2);
    blood.cost_for_turn = 1;
    update_card_cost(blood, -1);
    SPIRE_ASSERT(blood.cost == 0 && blood.cost_for_turn == 1);

    Game g{};
    reset(g, 101);
    const int windmill = create_card(g, make_card(CardId::WindmillStrike));
    card_onRetained(g, windmill);
    SPIRE_ASSERT(g.cards[windmill].base_damage == 11);
    const Card copy = card_makeStatEquivalentCopy(g, windmill);
    SPIRE_ASSERT(copy.base_damage == 11 && copy.upgrades == 0);
    card_upgrade(g.cards[windmill]);
    const Card upgraded_copy = card_makeStatEquivalentCopy(g, windmill);
    SPIRE_ASSERT(upgraded_copy.base_damage == 14);
    return 0;
}
