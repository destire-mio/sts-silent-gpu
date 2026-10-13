#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Slimed : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Status, CardTarget::Self, 1, -1, -1, -1, 0, 0, 0, true, false, CardRarity::Common};
    }
    static SPIRE_FN void use(Game&, int, int) {}
    static SPIRE_FN void upgrade(Card&) {}
};
}
