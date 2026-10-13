#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Dazed : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Status, CardTarget::None, -2, -1, -1, -1, 0, 0, 0, false, true};
    }
    static SPIRE_FN void use(Game&, int, int) {}
    static SPIRE_FN void upgrade(Card&) {}
};
}
