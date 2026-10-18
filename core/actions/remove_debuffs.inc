#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct RemoveDebuffsAction : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("RemoveDebuffsAction.c", creature, effect.target)
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = no_target; }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        const Powers& powers = get_creature(g, e.target).powers;
        for (int i = 0; i < powers.size; ++i) {
            if (!power_isDebuff(powers.at(g.power_objects, i), e.target)) continue;
            Effect remove{};
            remove.kind = EffectKind::RemovePower;
            remove.source = remove.target = e.target;
            remove.power = powers.at(g.power_objects, i).id;
            add_top(g, remove);
        }
        e.done = true;
    }
};
}
