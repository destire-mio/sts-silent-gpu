#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Tingsha : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void onManualDiscard(Game& g, int relic) {
        Effect show{};
        show.kind = EffectKind::RelicAboveCreature;
        show.extra = relic;
        add_bottom(g, show);
        Effect hit{};
        hit.kind = EffectKind::DamageRandomEnemy;
        hit.amount = hit.info_base = 3;
        hit.damage_type = DamageType::Thorns;
        add_bottom(g, hit);
    }
};
}

namespace spire::units::actions {
struct RelicAboveCreature : ActionHooks {
    SPIRE_STATE(RelicAboveCreature, int relic; bool used;)
    SPIRE_SNAPSHOT("RelicAboveCreatureAction.relic", relic, effect.state.RelicAboveCreature.relic)
    static SPIRE_FN constexpr ActionData data() { return {false, "TEXT"}; }
    static SPIRE_FN void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.state.RelicAboveCreature.relic = e.extra;
    }
    static SPIRE_FN void execute(Game&, Effect& e) { e.state.RelicAboveCreature.used = true; }
};
}
