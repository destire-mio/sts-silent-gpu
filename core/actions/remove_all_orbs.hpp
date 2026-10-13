#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct RemoveAllOrbs : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    static SPIRE_FN void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    static SPIRE_FN void execute(Game& g, const Effect&) {
        while (filled_orb_count(g) > 0) remove_next_orb(g);
    }
};
}
