#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct Toolbox : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStartPreDraw(Game& g, int relic) {
        Effect show{};
        show.kind = EffectKind::RelicAboveCreature;
        show.extra = relic;
        add_bottom(g, show);
        Effect choose{};
        choose.kind = EffectKind::ChooseOneColorless;
        add_bottom(g, choose);
    }
};
}
