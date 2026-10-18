#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct VoidCard : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Status, CardTarget::None, -2, -1, -1, -1, 0, 0, 0, false, true};
    }
    static SPIRE_FN void use(Game&, int, int) {

    }
    static SPIRE_FN void triggerWhenDrawn(Game& g, int) {
        Effect energy{};
        energy.kind = EffectKind::LoseEnergy;
        energy.amount = 1;
        add_bottom(g, energy);
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::VoidCard); }
    static SPIRE_FN void upgrade(Card&) {
    }
};
}
