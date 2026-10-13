#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ModifyDamage : ActionHooks {
    SPIRE_STATE(ModifyDamage, uint32_t uuid;)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        bool found[card_capacity]{};
        SPIRE_ASSERT(g.player.card_in_use >= 0);
        found[g.player.card_in_use] = true;
        const PileId piles[] = {PileId::Draw, PileId::Discard, PileId::Exhaust, PileId::Limbo, PileId::Hand};
        for (int p = 0; p < 5; ++p) {
            const Pile& pile = get_pile(g, piles[p]);
            for (int i = 0; i < pile.size; ++i) found[pile[i]] = true;
        }
        for (int i = 0; i < g.cards.size; ++i)
            if (found[i] && g.cards[i].uuid == e.state.ModifyDamage.uuid)
                g.cards[i].base_damage = max(0, g.cards[i].base_damage + e.amount);
    }
};
}
