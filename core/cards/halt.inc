#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Halt : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 0, -1, 3, 9, 0, 1, 5, false, false};
        d.initial_block = 3;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        applyPowers(g, card);
        Effect e{};
        e.kind = EffectKind::Halt;
        e.source = no_target;
        e.amount = g.cards[card].block;
        e.extra = g.cards[card].magic;
        add_bottom(g, e);
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        Card& c = get_card(g, card);
        const int difference = 6 + c.upgrades * 4;
        // Halt.applyPowers calculates the Wrath block before restoring baseBlock.
        c.base_block_adjust += difference;
        c.base_magic = card_baseBlock(c);
        apply_card_powers(g, card);
        c.magic = c.block;
        c.base_block_adjust -= difference;
        apply_card_powers(g, card);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic = card_baseBlock(card) + 6 + card.upgrades * 4;
        }
    }
};
}
