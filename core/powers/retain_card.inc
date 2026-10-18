#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct RetainCard : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int owner, bool is_player) {
        if (is_player && !g.hand.empty() && !has_relic(g, RelicId::RunicPyramid) && !has_power(g, g.player.creature, PowerId::Equilibrium)) {
            Effect e{};
            e.kind = EffectKind::RetainCards;
            e.source = owner;
            e.amount = p.amount;
            add_bottom(g, e);
        }
    }
};
}
