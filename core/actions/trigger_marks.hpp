#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct TriggerMarks : ActionHooks {
    SPIRE_WAITING_RULE("TriggerMarksAction: waiting for a ported caller with three-seed oracle replay")
    SPIRE_SNAPSHOT("TriggerMarksAction.card", card, effect.card)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int owner = g.monster_order[i];
            Powers &powers = g.monsters[owner].creature.powers;
            for (int j = 0; j < powers.size; ++j)
                power_triggerMarks(g, powers.at(g.power_objects, j), owner, e.card);
        }
    }
};
}
