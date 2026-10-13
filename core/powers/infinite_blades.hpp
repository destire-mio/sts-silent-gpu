#pragma once
#include "../hooks.hpp"
#include "../cards/shiv.hpp"

namespace spire::units::powers {
struct InfiniteBlades : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int) {
        if (!monsters_basically_dead(g)) {
            Effect shivs{};
            shivs.kind = EffectKind::MakeTempCardInHand;
            shivs.card = create_card(g, cards::Shiv::make(g));
            shivs.amount = p.amount;
            action_initialize_state(shivs);
            shivs.payload.state.MakeTempCardInHand.other_card_not_in_center = true;
            add_bottom(g, shivs);
        }
    }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) {
        p.amount += amount;
    }
};
}
