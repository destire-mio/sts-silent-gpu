#pragma once
#include "core/hooks.hpp"

namespace spire::units::potions {
struct GamblersBrew : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {0, CardTarget::Self, true}; }
    static SPIRE_FN void use(Game& g, int, int) {
        if (!g.hand.empty()) {
            Effect e{};
            e.kind = EffectKind::GamblingChipAction;
            e.source = player_target;
            e.flag = true;
            add_bottom(g, e);
        }
    }
};
}
