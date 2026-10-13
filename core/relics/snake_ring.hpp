#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::actions {
struct RelicAboveCreature : ActionHooks {
    SPIRE_STATE(RelicAboveCreature, int relic; bool used;)
    SPIRE_SNAPSHOT("RelicAboveCreatureAction.relic", relic, effect.state.RelicAboveCreature.relic)
    static SPIRE_FN constexpr ActionData data() { return {false, "TEXT"}; }
    static SPIRE_FN Effect make(int owner, int relic) {
        Effect e{};
        e.kind = EffectKind::RelicAboveCreature;
        action_initialize_state(e);
        e.source = e.target = owner;
        e.state.RelicAboveCreature.relic = relic;
        return e;
    }
    static SPIRE_FN void execute(Game&, Effect& e) { e.state.RelicAboveCreature.used = true; }
};
}

namespace spire {
SPIRE_FN inline Effect relic_above_creature(int owner, int relic) {
    return units::actions::RelicAboveCreature::make(owner, relic);
}
}

namespace spire::units::relics {
struct SnakeRing : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1}; }
    static SPIRE_FN void atBattleStart(Game& g, int) {
        add_bottom(g, units::actions::Draw::construct(2));
    }
};
}
