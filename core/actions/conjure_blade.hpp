#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ConjureBlade : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_STATE(ConjureBlade, int energyOnUse; bool freeToPlayOnce; bool upgraded;)
    SPIRE_SNAPSHOT("ConjureBladeAction.multiDamage", scalar, nullptr)
    SPIRE_SNAPSHOT("ConjureBladeAction.p", player)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        int count =
            e.payload.state.ConjureBlade.energyOnUse < 0 ? g.player.energy : e.payload.state.ConjureBlade.energyOnUse;
        if (has_relic(g, "Chemical X"))
            count += 2;
        Card card = make_card(card_id("Expunger"));
        card.base_magic = card.magic = count;
        Effect a{};
        a.kind = EffectKind::MakeTempCardInDrawPile;
        action_initialize_state(a);
        a.source = a.target = no_target;
        a.card = create_card(g, card);
        a.amount = 1;
        a.payload.state.MakeTempCardInDrawPile.random_spot = true;
        add_bottom(g, a);
        if (!e.payload.state.ConjureBlade.freeToPlayOnce)
            g.player.energy = 0;
    }
};
}
