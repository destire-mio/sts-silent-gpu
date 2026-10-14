#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct MentalFortress : PowerHooks {
    SPIRE_WAITING_RULE("MentalFortressPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onChangeStance(Game& g, Power& p, int owner, int old_stance, int new_stance) {
        if (old_stance != new_stance) {
            Effect e{};
            e.kind = EffectKind::Block;
            e.source = e.target = owner;
            e.amount = p.amount;
            add_bottom(g, e);
        }
    }
};
}
