#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Nirvana : PowerHooks {
    SPIRE_WAITING_RULE("NirvanaPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onScry(Game& g, Power& p, int owner) {
        Effect e{};
        e.kind = EffectKind::Block;
        e.source = no_target;
        e.target = owner;
        e.amount = p.amount;
        add_bottom(g, e);
    }
};
}
