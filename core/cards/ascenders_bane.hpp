#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct AscendersBane : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef data{CardType::Curse, CardTarget::None, -2, -1, -1, -1, 0, 0, 0, false, true};
        data.purgeable = false;
        return data;
    }
    static SPIRE_FN void use(Game&, int, int) {}
    static SPIRE_FN void upgrade(Card&) {}
};
}
