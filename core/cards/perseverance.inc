#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Perseverance : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 1, -1, 5, 2, 0, 2, 1, false, false};
        d.self_retain = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
    }
    SPIRE_SNAPSHOT("AbstractCard.upgradedBlock", scalar, card.upgrades > 0 || card.base_block_adjust != 0)
    static SPIRE_FN void onRetained(Game& g, int card) {
        Card& c = g.cards[card];
        c.base_block_adjust += c.magic;
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
