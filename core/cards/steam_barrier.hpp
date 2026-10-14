#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct SteamBarrier : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, 6, -1, 0, 2, 0, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
        Effect e{};
        e.kind = EffectKind::ModifyBlock;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.amount = -1;
        e.payload.state.ModifyBlock.uuid = g.cards[card].uuid;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
