#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct CultistMask : RelicHooks {
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        queue_relic_above(g, relic, player_target, false);
        Effect sound{};
        sound.kind = EffectKind::SFX;
        add_bottom(g, sound);
        Effect talk{};
        talk.kind = EffectKind::Talk;
        add_bottom(g, talk);
    }
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
};
}
