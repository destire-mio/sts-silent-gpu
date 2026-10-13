#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct MayhemPlay : ActionHooks {
    SPIRE_SNAPSHOT("MayhemPower$1.this$0", power_reference, player_target, effect.extra)
    static SPIRE_FN constexpr ActionData data() { return {}; }
    static SPIRE_FN void execute(Game& g, Effect&) {
        Effect play{};
        play.kind = EffectKind::PlayTopCardAction;
        play.target = random_living_monster(g);
        add_bottom(g, play);
    }
};
}

namespace spire::units::powers {
struct Mayhem : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& power, int) {
        for (int i = 0; i < power.amount; ++i) {
            Effect e{};
            e.kind = EffectKind::MayhemPlay;
            e.source = e.target = no_target;
            e.extra = power.instance;
            add_bottom(g, e);
        }
    }
};
}
