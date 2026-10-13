#pragma once
#include "../hooks.hpp"
#include "../actions/ritual_dagger.hpp"

namespace spire::units::cards {
struct RitualDagger : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 15, -1, 3, 0, 0, 2, true, false, false, 15};
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
        get_card(g, card).base_block = get_card(g, card).misc;
        apply_card_powers(g, card);
    }
};
}
