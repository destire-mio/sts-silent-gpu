#pragma once
#include "../hooks.hpp"
#include "../actions/change_stance.hpp"

namespace spire::units::powers {
struct Devotion : PowerHooks {
    SPIRE_WAITING_RULE("DevotionPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurnPostDraw(Game& g, Power& p, int owner) {
        if (!has_power(g, g.player.creature, PowerId::Mantra) && p.amount >= 10)
            add_bottom(g, units::actions::ChangeStance::make(Stance::Divinity));
        else
            queue_power(g, owner, owner, PowerId::Mantra, p.amount);
    }
};
}
