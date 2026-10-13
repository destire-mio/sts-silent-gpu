#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Magnetism : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) {
        p.amount += amount;
    }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int) {
        if (monsters_basically_dead(g)) return;
        for (int i = 0; i < p.amount; ++i) {
            Effect e{};
            e.kind = EffectKind::MakeTempCardInHand;
            e.card = create_card(g, make_card(card_id(random_card_in_combat(g, -1, true))));
            e.amount = 1;
            e.state.MakeTempCardInHand.other_card_not_in_center = true;
            add_bottom(g, e);
        }
    }
};
}
