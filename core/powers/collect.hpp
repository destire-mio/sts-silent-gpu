#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Collect : PowerHooks {
    static SPIRE_FN constexpr PowerData data() {
        PowerData d{};
        d.turn_based = true;
        return d;
    }
    static SPIRE_FN void initialPower(Game &, Power& p, int) { p.amount = min(999, p.amount); }
    static SPIRE_FN void stackPower(Game &, Power& p, int, int amount) { p.amount = min(999, p.amount + amount); }
    static SPIRE_FN void onEnergyRecharge(Game& g, Power& p, int owner) {
        Effect e{};
        e.kind = EffectKind::MakeTempCardInHand;
        action_initialize_state(e);
        e.card = create_card(g, make_card(card_id("Miracle"), 1));
        e.amount = 1;
        e.payload.state.MakeTempCardInHand.other_card_not_in_center = false;
        add_bottom(g, e);
        if (p.amount <= 1) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = e.target = owner;
            e.power = PowerId::Collect;
            add_bottom(g, e);
        } else {
            Effect e{};
            e.kind = EffectKind::ReducePower;
            e.source = e.target = owner;
            e.power = PowerId::Collect;
            e.amount = 1;
            add_bottom(g, e);
        }
    }
};
}
