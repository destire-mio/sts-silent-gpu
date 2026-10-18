#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct GeneticAlgorithm : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 1, 2, 0, 0, 1, true, false, false, false, 1};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::IncreaseMisc;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.extra = g.cards[card].magic;
        e.payload.state.IncreaseMisc.uuid = g.cards[card].uuid;
        add_bottom(g, e);
        queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        Card& c = get_card(g, card);
        c.base_block_adjust += c.misc - card_baseBlock(c);
        apply_card_powers(g, card);
    }
};
}
