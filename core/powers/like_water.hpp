#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct LikeWater : PowerHooks {
    SPIRE_WAITING_RULE("LikeWaterPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void stackPower(Game &, Power& p, int, int amount) { p.amount = min(999, p.amount + amount); }
    static SPIRE_FN void atEndOfTurnPreEndTurnCards(Game& g, Power& p, int owner, bool is_player) {
        if (is_player && g.player.stance == Stance::Calm) {
            Effect e{};
            e.kind = EffectKind::Block;
            e.source = e.target = owner;
            e.amount = p.amount;
            add_bottom(g, e);
        }
    }
};
}
