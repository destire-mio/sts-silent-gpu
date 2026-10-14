#pragma once
#include "../hooks.hpp"
#include "draw.hpp"

namespace spire::units::actions {
struct CompileDriver : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        bool present[4]{};
        int count = 0;
        for (int i = 0; i < g.orbs.slots.size; ++i)
            if (g.orbs.slots[i] >= 0)
                present[static_cast<int>(g.orbs.kind(g.orbs.slots[i]))] = true;
        for (bool value : present)
            if (value)
                ++count;
        if (e.amount * count > 0)
            add_top(g, units::actions::Draw::construct(e.amount * count, true, -1, e.source));
    }
};
}
