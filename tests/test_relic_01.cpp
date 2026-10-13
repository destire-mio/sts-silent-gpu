#include "core/engine.hpp"
#include <cstdio>
using namespace spire;

int main() {
    Game first{}, second{};
    equip_relic(first, RelicId::CentennialPuzzle);
    equip_relic(second, RelicId::CentennialPuzzle);
    first.room_in_combat = true;
    relic_wasHPLost(first, 0, 1);
    SPIRE_ASSERT(first.unit_statics.CentennialPuzzle.usedThisCombat);
    SPIRE_ASSERT(!second.unit_statics.CentennialPuzzle.usedThisCombat);
    SPIRE_ASSERT(unequip_relic(first, RelicId::CentennialPuzzle));
    SPIRE_ASSERT(first.unit_statics.CentennialPuzzle.usedThisCombat);
    reset(first, 101);
    SPIRE_ASSERT(!first.unit_statics.CentennialPuzzle.usedThisCombat);
    std::puts("Relic static state survives removal and stays local to one Game");
}
