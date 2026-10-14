#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ModifyBlock : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(ModifyBlock, uint32_t uuid;)
    SPIRE_SNAPSHOT("ModifyBlockAction.uuid", uuid, effect.payload.state.ModifyBlock.uuid)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        bool found[card_capacity]{};
        if (g.player.card_in_use >= 0)
            found[g.player.card_in_use] = true;
        const Pile *piles[] = {&g.hand, &g.draw, &g.discard, &g.exhaust, &g.limbo};
        for (const Pile *pile : piles)
            for (int i = 0; i < pile->size; ++i)
                found[(*pile)[i]] = true;
        for (int i = 0; i < g.cards.size; ++i) {
            Card& c = g.cards[i];
            if (found[i] && c.uuid == e.payload.state.ModifyBlock.uuid) {
                c.base_block_adjust += e.amount;
                // ModifyBlockAction.update clamps baseDamage, not baseBlock.
                if (c.base_damage < 0)
                    c.base_damage = 0;
            }
        }
    }
};
}
