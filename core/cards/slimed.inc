#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Slimed : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Status, CardTarget::Self, 1, -1, -1, -1, 0, 0, 0, true, false};
        d.updates_cost = true;
        return d;
    }
    static SPIRE_FN void use(Game&, int, int) {}
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::Slimed); }
    static SPIRE_FN void upgrade(Card&) {}
};
}
