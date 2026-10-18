#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Injury : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Curse, CardTarget::None, -2, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game&, int, int) {}
    static SPIRE_FN void upgrade(Card&) {}
};
}
