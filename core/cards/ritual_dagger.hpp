#pragma once
#include "../hooks.hpp"
#include "../actions/ritual_dagger.hpp"

namespace spire::units::cards {
struct RitualDagger : CardHooks {
    SPIRE_STATE(RitualDagger, bool block_from_misc;)
    static SPIRE_FN int baseBlock(const Card& card) { return card.state.RitualDagger.block_from_misc ? card.misc : -1; }
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 15, -1, 3, 0, 0, 2, true, false, false, false, 15};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        DamageInfo info{};
        info.source = player_target;
        info.amount = info.info_base = g.cards[card].damage;
        add_bottom(g, actions::RitualDagger::construct(target, info, g.cards[card].magic, g.cards[card].uuid));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        // RitualDagger.applyPowers writes baseBlock, not baseDamage.
        get_card(g, card).state.RitualDagger.block_from_misc = true;
        apply_card_powers(g, card);
    }
};
}
