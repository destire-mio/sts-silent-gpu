#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct BattleHymn : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int) {
        if (!monsters_basically_dead(g)) {
            Effect e{};
            e.kind = EffectKind::MakeTempCardInHand;
            action_initialize_state(e);
            e.card = create_card(g, make_card(CardId::Smite, 0));
            e.amount = p.amount;
            e.payload.state.MakeTempCardInHand.other_card_not_in_center = true;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN void stackPower(Game &, Power& p, int, int amount) { p.amount += amount; }
};
}
