#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct WaveOfTheHand : PowerHooks {
    SPIRE_WAITING_RULE("WaveOfTheHandPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onGainedBlock(Game& g, Power& p, int, float amount) {
        if (amount > 0.0f)
            for (int i = 0; i < g.monster_order.size; ++i)
                queue_power(g, player_target, g.monster_order[i], PowerId::Weak, p.amount);
    }
    static SPIRE_FN void atEndOfRound(Game& g, Power &, int owner) {
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.source = e.target = owner;
        e.power = PowerId::WaveOfTheHand;
        add_bottom(g, e);
    }
};
}
