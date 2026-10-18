#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Wound : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Status, CardTarget::None, -2, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game&, int, int) {

    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::Wound); }
    static SPIRE_FN void upgrade(Card&) {
    }
};
}
